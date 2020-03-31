// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "spinlock.h"

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

int allocated[512];
int allocated_count = 1;
struct run *skipped;
int flag = -1;

extern char end[]; // first address after kernel loaded from ELF file

// Initialize free list of physical pages.
void
kinit(void)
{
  char *p;
  flag = 0;
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

  if (flag == 0) {
      r->next = kmem.freelist;
      kmem.freelist = r;
  } 
  if (flag == 1){
      r->next = skipped;
      kmem.freelist = r;
  }
  for (int i = 0; i < allocated_count; i++) {
    if (allocated[i] == (int)r) {
      for (int j = i; j < allocated_count - 1; j++)
        allocated[j] = allocated[j + 1];
      allocated_count--;
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
  flag = 1;
  acquire(&kmem.lock);
  r = kmem.freelist;
  skipped = kmem.freelist;
  if(r) {
    skipped = r->next;
    kmem.freelist = r->next->next;
    allocated[allocated_count] = (int)r;
    allocated_count++;
  }
  release(&kmem.lock);
  return (char*)r;
}

int
dump_allocated (int *frames, int numframes) {
    if (numframes > allocated_count)
        return -1;
    for (int i = allocated_count; i >= allocated_count - numframes; i--) {
        *frames = allocated[i];
        frames++;
    }
    return 0;
}
