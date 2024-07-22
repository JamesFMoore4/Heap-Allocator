#ifndef MY_HEAP_ALLOC
#define MY_HEAP_ALLOC

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>

/* User interface */
void* my_alloc(unsigned size);
void my_free(void* bp);

/* Allocator helper functions */
static int my_alloc_init(void);
static void* ext(unsigned bytes);
static void place(void* bp, unsigned asize);
static void* fit(unsigned asize);
static void coal(void);
static void seg_lst_add(void* bp);
static void seg_lst_rem(void* bp);
static int seg_lst_index(unsigned size, int fit);

/* Memory model functions */
static void mmodel_init(void);
static void* mem_sbrk(int incr);

/* Constant macros */
#define WSIZE 4
#define DSIZE 8
#define MIN_BLOCK_SIZE 16
#define MAX_HEAP_SIZE (20 * (1 << 20))  // 20 MB
#define CHUNK_SIZE (1 << 12)  // 4 KB
#define NUM_CLASSES 9

/* Pointer handling macros */
#define GET(ptr) (*(unsigned*)(ptr))
#define PUT(ptr, val) (*(unsigned*)(ptr) = (val))
#define PACK(val1, val2) ((val1) | (val2))
#define SIZE(ptr) (GET(ptr) & ~0x7)
#define ALLOC(ptr) (GET(ptr) & 0x1)
#define HDRP(blkp) ((char*)(blkp) - WSIZE)
#define FTRP(blkp) ((char*)(blkp) + SIZE(HDRP(blkp)) - DSIZE)
#define PREV_BLKP(blkp) ((char*)(blkp) - SIZE((char*)(blkp) - DSIZE))
#define NEXT_BLKP(blkp) ((char*)(blkp) + SIZE((char*)(blkp) - WSIZE))

#endif

