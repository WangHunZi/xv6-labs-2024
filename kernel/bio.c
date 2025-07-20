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
  struct buf buf[NBUF];
  struct bucket bucket[NBUCKETS];
} bcache;

void
binit(void)
{
  for (int i = 0; i < NBUCKETS; i ++) {
    bcache.bucket[i].head = 0;
    char bname[16];
    snprintf(bname, sizeof(bname), "bucket%d", i);
    initlock(&bcache.bucket[i].lock, bname);
  }
}

static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  struct bucket *bucket = &bcache.bucket[blockno % NBUCKETS];
  acquire(&bucket->lock);
  for (b = bucket->head; b; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bucket->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bucket->lock);

  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    acquire(&bucket->lock);
    if (b->refcnt == 0 && b->disk == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      b->next = 0;

      b->next = bucket->head;
      bucket->head = b;
      release(&bucket->lock);
      acquiresleep(&b->lock);
      return b;
    }
    release(&bucket->lock);
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
  if (b->refcnt == 0 && b->disk == 0) {
    struct buf **prev = &bucket->head;
    while (*prev != b)
      prev = &((*prev)->next);
    *prev = b->next;
  }
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


