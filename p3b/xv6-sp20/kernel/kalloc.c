// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "spinlock.h"
#include "rand.c"

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

int alloc[512];
int alloc_count = 1;
int free = 0;

extern char end[]; // first address after kernel loaded from ELF file

// Initialize free list of physical pages.
void
kinit(void)
{
  char *p;
  initlock(&kmem.lock, "kmem");
  p = (char*)PGROUNDUP((uint)end);
  for(; p + PGSIZE <= (char*)PHYSTOP; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || (uint)v >= PHYSTOP) 
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  acquire(&kmem.lock);
  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;
  free++;
  for (int i = 0; i < alloc_count; i++) {
    if (alloc[i] == (int)r) {
      for (int j = i; j < alloc_count - j; j++)
        alloc[j] = alloc[j + 1];
      alloc_count--;
    }
  }
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;
  int rand = xv6_rand();
  int rm = rand % free;
  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    struct run *prev;
    if (r != NULL && rm == 0) {
        kmem.freelist = r->next;
    } else {
        int cnt = 0;
        while (r != NULL && cnt != rm) {
            prev = r;
            r = r->next;
            cnt++;
        }
        prev->next = r->next;
    }
    alloc[alloc_count] = (int)r;
    alloc_count++;
    free--;
  }
  release(&kmem.lock);
  return (char*)r;
}

int
dump_allocated (int *frames, int numframes) {
    if (numframes > alloc_count)
        return -1;
    for (int i = alloc_count; i >= alloc_count - numframes; i--) {
        *frames = alloc[i];
        frames++;
    }
    return 0;
}
