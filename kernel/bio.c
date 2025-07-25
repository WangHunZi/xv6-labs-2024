// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct bucket {
  struct spinlock lock;
  struct buf *head;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct bucket bucket[NBUCKETS];
} bcache;

void
binit(void)
{
  struct bucket *bkt = bcache.bucket;
  for (struct buf *b = bcache.buf; b < (bcache.buf + NBUF); b ++) {
    b->dev = 0;
    b->blockno = 0;
    b->next = bkt->head;
    bkt->head = b;
    initlock(&bcache.lock, "bcache");
    initsleeplock(&b->lock, "buffer");
  }

  for (bkt = bcache.bucket; bkt < bcache.bucket + NBUCKETS; bkt ++) {
    initlock(&bkt->lock, "bcache");
  }
}

static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  struct bucket *bucketA = &bcache.bucket[blockno % NBUCKETS];
  acquire(&bucketA->lock);
  for (b = bucketA->head; b; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bucketA->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  for (b = bucketA->head; b; b = b->next) {
    if (b->refcnt == 0 && b->disk == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bucketA->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bucketA->lock);

  for (b = bcache.buf; b < (bcache.buf + NBUF); b++) {
    struct bucket *bucketB = &bcache.bucket[b->blockno % NBUCKETS];
    if (bucketA == bucketB)
      continue;

    acquire(&bucketA->lock);
    acquire(&bucketB->lock);

    if (b->refcnt == 0 && b->disk == 0) {
      struct buf **pp = &bucketB->head;
      while (*pp) {
        if (*pp == b) {
          *pp = b->next;
          break;
        }
        pp = &(*pp)->next;
      }

      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;

      b->next = bucketA->head;
      bucketA->head = b;

      release(&bucketB->lock);
      release(&bucketA->lock);
      acquiresleep(&b->lock);
      return b;
    }
    release(&bucketB->lock);
    release(&bucketA->lock);
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  struct bucket *bucket = &bcache.bucket[b->blockno % NBUCKETS];
  acquire(&bucket->lock);
  b->refcnt--;
  release(&bucket->lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.bucket[b->blockno % NBUCKETS].lock);
  b->refcnt++;
  release(&bcache.bucket[b->blockno % NBUCKETS].lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.bucket[b->blockno % NBUCKETS].lock);
  b->refcnt--;
  release(&bcache.bucket[b->blockno % NBUCKETS].lock);
}


