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

#define NUM_BUCKET 13
struct {
  struct spinlock lock;
  struct spinlock bucket_locks[NUM_BUCKET];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf bucket_heads[NUM_BUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for (int i = 0; i < NUM_BUCKET; i++){
    initlock(&(bcache.bucket_locks[i]), "bcache.bucket");
  }

  // Create linked list of buffers
  for (int i = 0; i < NUM_BUCKET; i++){
    bcache.bucket_heads[i].prev = &(bcache.bucket_heads[i]);
    bcache.bucket_heads[i].next = &(bcache.bucket_heads[i]);
  }

  if(NUM_BUCKET > NBUF){
    panic("binit");
  }

  int j = 0;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    b->ticks = ticks;
    b->refcnt = 0;
    b->next = bcache.bucket_heads[j].next;
    b->prev = &(bcache.bucket_heads[j]);
    initsleeplock(&b->lock, "buffer"); //every buffer has a sleeplock
    bcache.bucket_heads[j].next->prev = b;
    bcache.bucket_heads[j].next = b;
    j = (j + 1) % NUM_BUCKET;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock);

  int bucket_id = blockno % NUM_BUCKET;
  acquire(&bcache.bucket_locks[bucket_id]);

  // Is the block already cached?
  for(b = bcache.bucket_heads[bucket_id].next; b != &bcache.bucket_heads[bucket_id]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket_locks[bucket_id]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket_locks[bucket_id]);


  struct buf * lru = (struct buf *)(0);
  int lru_bucket = -1;

  
  //borrow one lru unused buffer
find_LRU:
  for (int i = 0; i < NUM_BUCKET; i++){
    for (b = bcache.bucket_heads[i].prev; b != &(bcache.bucket_heads[i]); b = b->prev){
      if(b->refcnt == 0){
          if(!lru){
            lru = b;
            lru_bucket = i;
          }else{
            if(lru->ticks > b->ticks){
              lru = b;
              lru_bucket = i;
            }
          }
      }
    }
  }
  if(lru_bucket == -1){
    panic("bget: no buffers");
  }
  if(lru_bucket == bucket_id){
    acquire(&bcache.bucket_locks[lru_bucket]);
    if(lru->refcnt != 0){
      release(&bcache.bucket_locks[lru_bucket]);
      goto find_LRU;
    }
  }else{
    acquire(&bcache.bucket_locks[bucket_id]);
    acquire(&bcache.bucket_locks[lru_bucket]);
    if(lru->refcnt != 0){
      release(&bcache.bucket_locks[bucket_id]);
      release(&bcache.bucket_locks[lru_bucket]);
      goto find_LRU;
    }
  }

  //delete lru from original bucket
  if(lru_bucket == bucket_id){
      lru->dev = dev;
      lru->blockno = blockno;
      lru->valid = 0;
      lru->refcnt = 1;
      lru->ticks = ticks;
      release(&bcache.bucket_locks[lru_bucket]);
      release(&bcache.lock);
      acquiresleep(&lru->lock);
      return lru;
  }else{
    
    lru->next->prev = lru->prev;
    lru->prev->next = lru->next;
    
    lru->next = bcache.bucket_heads[bucket_id].next;
    lru->prev = &bcache.bucket_heads[bucket_id];
    bcache.bucket_heads[bucket_id].next->prev = lru;
    bcache.bucket_heads[bucket_id].next = lru;

    lru->dev = dev;
    lru->blockno = blockno;
    lru->valid = 0;
    lru->refcnt = 1;
    lru->ticks = ticks;
    release(&bcache.bucket_locks[lru_bucket]);
    release(&bcache.bucket_locks[bucket_id]);
    release(&bcache.lock);
    acquiresleep(&lru->lock);
    return lru;

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

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->ticks = ticks;
  }

}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  int bucket_id = b->blockno % NUM_BUCKET;
  acquire(&bcache.bucket_locks[bucket_id]);

  b->refcnt++;
  release(&bcache.bucket_locks[bucket_id]);
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  int bucket_id = b->blockno % NUM_BUCKET;
  acquire(&bcache.bucket_locks[bucket_id]);

  b->refcnt--;
  release(&bcache.bucket_locks[bucket_id]);
  release(&bcache.lock);
}


