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
} kmem;


// yyy

#define OFFSET_UNIT PGSIZE

uint8 reference_counts[(PHYSTOP-KERNBASE)/PGSIZE]; // for pa page reference count.


uint8 get_pa_ref_count(uint64 pa){
  if(pa>=(uint64)end && pa<=PHYSTOP){
    uint64 offset = pa - (uint64)end;
    int index = offset / OFFSET_UNIT;
    return reference_counts[index];
  } else{
    return 0;
  }

}

void init_pa_ref_count(uint64 pa){
  if(pa>=(uint64)end && pa<=PHYSTOP) {
    uint64 offset = pa - (uint64)end;
    int index = offset / OFFSET_UNIT;
    reference_counts[index] = 1;
  }
}

void increment_pa_ref_count_using_va(pagetable_t pagetable, uint64 va){
  uint64 pa = walkaddr(pagetable, va);
  increment_pa_ref_count(pa);
}
void increment_pa_ref_count(uint64 pa){
  if(pa < (uint64)end || pa > PHYSTOP) return;

  uint64 offset = pa - (uint64)end;
  int index = offset / OFFSET_UNIT;
  reference_counts[index] += 1;
}

void decrement_pa_ref_count_using_va(pagetable_t pagetable, uint64 va){
  uint64 pa = walkaddr(pagetable, va);
  decrement_pa_ref_count(pa);
}
void decrement_pa_ref_count(uint64 pa){
  if(pa < (uint64)end || pa > PHYSTOP) return;

  uint64 offset = pa - (uint64)end;
  uint32 index = offset / OFFSET_UNIT;
  if (reference_counts[index] > 0){
    reference_counts[index] -= 1;
    // yyy auto kfree page which ref count is 0.
//    if (reference_counts[index] == 0){
//      kfree((void*)pa);
//    }
  }
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);

}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  //yyy???????????
  decrement_pa_ref_count((uint64)pa);
  uint8 ref_count = get_pa_ref_count((uint64)pa);
  if(ref_count > 0){
    return;
  }
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
    //yyy
  if(r)
    init_pa_ref_count((uint64)r);

  return (void*)r;
}
