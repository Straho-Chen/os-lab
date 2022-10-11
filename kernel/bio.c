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

#define NBUCKETS 17

struct
{
  struct spinlock lock;
  struct spinlock bucketlock[NBUCKETS];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf hashbucket[NBUCKETS];
} bcache;

uint hashkey(uint blockno)
{
  return blockno % NBUCKETS;
}

void binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for (int i = 0; i < NBUCKETS; i++)
  {
    // Create linked list of buffers
    initlock(&bcache.bucketlock[i], "bcache.bucket");
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];
  }

  for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  {
    int key = hashkey(b->blockno);
    b->next = bcache.hashbucket[key].next;
    b->prev = &bcache.hashbucket[key];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbucket[key].next->prev = b;
    bcache.hashbucket[key].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
  struct buf *b;

  int key = hashkey(blockno);
  acquire(&bcache.bucketlock[key]);

  // Is the block already cached?
  for (b = bcache.hashbucket[key].next; b != &bcache.hashbucket[key]; b = b->next)
  {
    if (b->dev == dev && b->blockno == blockno)
    {
      b->refcnt++;
      release(&bcache.bucketlock[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for (b = bcache.hashbucket[key].prev; b != &bcache.hashbucket[key]; b = b->prev)
  {
    if (b->refcnt == 0)
    {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.bucketlock[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  acquire(&bcache.lock);
  for (int i = 0; i < NBUCKETS; i++)
  {
    if (i != key)
    {
      acquire(&bcache.bucketlock[i]);
      for (b = bcache.hashbucket[i].prev; b != &bcache.hashbucket[i]; b = b->prev)
      {
        if (b->refcnt == 0)
        {
          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;
          b->refcnt = 1;

          b->prev->next = b->next;
          b->next->prev = b->prev;
          release(&bcache.bucketlock[i]);

          b->next = bcache.hashbucket[key].next;
          b->prev = &bcache.hashbucket[key];
          bcache.hashbucket[key].next->prev = b;
          bcache.hashbucket[key].next = b;

          release(&bcache.lock);
          release(&bcache.bucketlock[key]);
          acquiresleep(&b->lock);
          return b;
        }
      }
      release(&bcache.bucketlock[i]);
    }
  }
  release(&bcache.lock);

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int key = hashkey(b->blockno);
  acquire(&bcache.bucketlock[key]);
  b->refcnt--;
  if (b->refcnt == 0)
  {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbucket[key].next;
    b->prev = &bcache.hashbucket[key];
    bcache.hashbucket[key].next->prev = b;
    bcache.hashbucket[key].next = b;
  }
  release(&bcache.bucketlock[key]);
}

void bpin(struct buf *b)
{
  int key = hashkey(b->blockno);
  acquire(&bcache.bucketlock[key]);
  b->refcnt++;
  release(&bcache.bucketlock[key]);
}

void bunpin(struct buf *b)
{
  int key = hashkey(b->blockno);
  acquire(&bcache.bucketlock[key]);
  b->refcnt--;
  release(&bcache.bucketlock[key]);
}
