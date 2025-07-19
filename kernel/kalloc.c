// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
  for(int i = 0; i < NCPU; i++) {
    char str[16];
    snprintf(str, sizeof(str), "kmem-%d", i);
    initlock(&kmem[i].lock, str);
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  uint64 average = PGROUNDDOWN(((uint64)pa_end - (uint64)pa_start) / NCPU);

  char *p = (char*)PGROUNDUP((uint64)pa_start);
  char *start = p;
  for (int hart = 0; hart < NCPU; hart++) {
    char *end = (char*)(start + average * (hart + 1));
    if (hart == (NCPU - 1)) {
      end = (char *)pa_end;
    }
    printf("range %d: %p to %p\n", hart, p, end);
    for(; p + PGSIZE <= end; p += PGSIZE) {
      struct run *r = (struct run*)p;
      r->next = kmem[hart].freelist;
      kmem[hart].freelist = r;
    }
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int hart = cpuid();
  pop_off();

  acquire(&kmem[hart].lock);
  r->next = kmem[hart].freelist;
  kmem[hart].freelist = r;
  release(&kmem[hart].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int hart = cpuid();
  pop_off();

  acquire(&kmem[hart].lock);
  r = kmem[hart].freelist;
  if(r)
    kmem[hart].freelist = r->next;
  release(&kmem[hart].lock);

  if(!r) {
    for (int i = 0; i < NCPU; i ++) {
      acquire(&kmem[i].lock);
      r = kmem[i].freelist;
      if (r) {
        kmem[i].freelist = r->next;
        release(&kmem[i].lock);
        break;
      }
      release(&kmem[i].lock);
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
