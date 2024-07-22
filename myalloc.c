#include "myalloc.h"

/* Memory model global variables */
static char* heap;
static char* mbrk;
static char* max_addr;

/* Allocator global variables */
static char* first_block = 0;
static char** seg_lst[] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
static unsigned classes[] = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096};

/* User interface */
void* my_alloc(unsigned size)
{
  if (!first_block)
  {
    if (!my_alloc_init())
    {
      errno = ENOMEM;
      return NULL;
    }
  }

  if (size > MAX_HEAP_SIZE - (MIN_BLOCK_SIZE + DSIZE))
  {
    errno = ENOMEM;
    return NULL;
  }

  unsigned asize;
  if (size <= DSIZE)
    asize = MIN_BLOCK_SIZE;
  else
    asize = DSIZE * ((size + DSIZE + (DSIZE-1)) / DSIZE);

  void* bp;
  if (!(bp = fit(asize)))
  {
    coal();
    if (!(bp = fit(asize)))
    {
      unsigned ext_amount = asize > CHUNK_SIZE ?
	CHUNK_SIZE * ((asize + CHUNK_SIZE + (CHUNK_SIZE-1)) / CHUNK_SIZE) :
	CHUNK_SIZE;
      if (!ext(ext_amount))
      {
	errno = ENOMEM;
	return NULL;
      }
      bp = fit(asize);
    }
  }
  place(bp, asize);
  return bp;
}

void my_free(void* bp)
{
  unsigned size = SIZE(HDRP(bp));
  PUT(HDRP(bp), PACK(size, 0x0));
  PUT(FTRP(bp), PACK(size, 0x0));
  *(char**)bp = NULL;
  seg_lst_add(bp);
}

/* Allocator helper functions */
static int my_alloc_init(void)
{
  mmodel_init();
  if (!(first_block = mem_sbrk(4 * WSIZE)))
    return 0;

  PUT(first_block, 0);
  first_block += WSIZE;
  PUT(first_block, PACK(DSIZE, 0x1));
  first_block += WSIZE;
  PUT(first_block, PACK(DSIZE, 0x1));
  PUT(first_block + WSIZE, 0x1);

  char* bp;
  if (!(bp = ext(CHUNK_SIZE)))
      return 0;

  seg_lst_add(bp);
  
  return 1;
}

static void* ext(unsigned bytes)
{
  /* Extend heap */
  unsigned words = bytes / WSIZE;
  unsigned size = words % 2 ? (words + 1) * WSIZE : words * WSIZE; // Size must be even to store alloc bit

  char* blkp;
  if (!(blkp = (char*)mem_sbrk(size)))
    return NULL;

  PUT(HDRP(blkp), size);
  PUT(FTRP(blkp), size);
  *(char**)blkp = NULL; // Set successor pointer
  PUT(HDRP(NEXT_BLKP(blkp)), 0x1);

  return (void*)blkp;
}

static void place(void* bp, unsigned asize)
{
  /* Allocate (and possibly split) blocks */
  unsigned prev_size = SIZE(HDRP(bp));
  unsigned size_diff =  prev_size - asize;

  seg_lst_rem(bp);
  
  if (size_diff < MIN_BLOCK_SIZE)
  {
    PUT(HDRP(bp), PACK(prev_size, 0x1));
    PUT(FTRP(bp), PACK(prev_size, 0x1));
    return;
  }

  PUT(HDRP(bp), PACK(asize, 0x1));
  PUT(FTRP(bp), PACK(asize, 0x1));

  char* next_bp = NEXT_BLKP(bp);
  PUT(HDRP(next_bp), PACK(size_diff, 0x0));
  PUT(FTRP(next_bp), PACK(size_diff, 0x0));
  *(char**)next_bp = NULL;
  seg_lst_add(next_bp);
}

static void* fit(unsigned asize)
{
  /* Return pointer to free block */
  int index = seg_lst_index(asize, 1);
  for(; !seg_lst[index] && index < NUM_CLASSES; index++);
  
  if (index == NUM_CLASSES)
    return NULL;

  return (void*)seg_lst[index];
}

static void coal(void)
{
  /* Coalesce all free blocks on the heap */
  char* ptr = NEXT_BLKP(first_block);
  while (SIZE(HDRP(ptr)))
  {
    if (!ALLOC(HDRP(ptr)))
    {
      unsigned total_size;
      char* next = NEXT_BLKP(ptr);
      char* prev = PREV_BLKP(ptr);
      /* Previous block and next block both free */
      if (!ALLOC(HDRP(prev)) && !ALLOC(HDRP(next)))
      {
	total_size = SIZE(HDRP(ptr)) + SIZE(HDRP(prev)) + SIZE(HDRP(next));
	
	seg_lst_rem(prev);
	seg_lst_rem(ptr);
	seg_lst_rem(next);
	
	PUT(HDRP(prev), PACK(total_size, 0x0));
	PUT(FTRP(prev), PACK(total_size, 0x0));
	*(char**)prev = NULL;
	seg_lst_add(prev);
	
	ptr = NEXT_BLKP(prev);
      }
      /* Previous block free, next block allocated */
      else if (!ALLOC(HDRP(prev)) && ALLOC(HDRP(next)))
      {
	total_size = SIZE(HDRP(ptr)) + SIZE(HDRP(prev));
	
	seg_lst_rem(prev);
	seg_lst_rem(ptr);
	
	PUT(HDRP(prev), PACK(total_size, 0x0));
	PUT(FTRP(prev), PACK(total_size, 0x0));
	*(char**)prev = NULL;
	seg_lst_add(prev);
	
	ptr = NEXT_BLKP(prev);
      }
      /* Previous block allocated, next block free */
      else if (ALLOC(HDRP(prev)) && !ALLOC(HDRP(next)))
      {
	total_size = SIZE(HDRP(ptr)) + SIZE(HDRP(next));
	
	seg_lst_rem(ptr);
	seg_lst_rem(next);
	
	PUT(HDRP(ptr), PACK(total_size, 0x0));
	PUT(FTRP(ptr), PACK(total_size, 0x0));
	*(char**)ptr = NULL;
	seg_lst_add(ptr);
	
	ptr = NEXT_BLKP(ptr);
      }
      /* Previous and next block both allocated */
      else
	ptr = NEXT_BLKP(ptr);
    }
    else
      ptr = NEXT_BLKP(ptr);
  }
}

static void seg_lst_add(void* bp)
{
  /* Add a free block to the segregated list */
  int index = seg_lst_index(SIZE(HDRP(bp)), 0);

  if (!seg_lst[index])
    seg_lst[index] = (char**)bp;
  else
  {
    char** ptr = seg_lst[index];
    for (; *ptr; ptr = (char**)*ptr);
    *ptr = (char*)bp;
  }
}

static void seg_lst_rem(void* bp)
{
  /* Remove entry from seg_lst */
  int index = seg_lst_index(SIZE(HDRP(bp)), 0);

  if (seg_lst[index] == (char**)bp)
  {
    seg_lst[index] = *(char***)bp;
    return;
  }

  char** ptr = seg_lst[index];
  for (; *ptr != (char*)bp; ptr = (char**)*ptr);
  *ptr = *(char**)bp;
}

static int seg_lst_index(unsigned size, int fit)
{
  /* 
     Determine index of size class.
     Fit determines whether the calling function
     is attempting to add/remove a block to/from seg_lst (0),
     or attempting to allocate a block (1).
  */
  for (int i = 0; i < NUM_CLASSES; i++)
  {
    if (fit)
    {
      if (size <= classes[i])
	return i;
    }
    else
    {
      if ((i == NUM_CLASSES - 1) ||
	  (size >= classes[i] && size < classes[i+1]))
	return i;
    }
  }
}

/* Memory model functions */
static void mmodel_init(void)
{
  heap = (char*)malloc(MAX_HEAP_SIZE);
  if (!heap)
  {
    fprintf(stderr, "Could not allocate heap memory\n");
    return;
  }
  mbrk = heap;
  max_addr = heap + MAX_HEAP_SIZE;
}

static void* mem_sbrk(int incr)
{
  char* prev_mbrk = mbrk;

  if (!heap)
    return NULL;
  
  if (incr < 0)
  {
    fprintf(stderr, "Heap size cannot be decreased.\n");
    return NULL;
  }

  if (mbrk + incr > max_addr)
  {
    errno = ENOMEM;
    fprintf(stderr, "Out of memory.\n");
    return NULL;
  }

  mbrk += incr;
  return (void*)prev_mbrk;
}

/* Testing functions */
void print_blocks(void)
{
  /*
    Prints the number, size, and allocated status of all
    blocks to the console 
  */
  size_t counter = 0;
  char* ptr = first_block;
  while (SIZE(HDRP(ptr)))
  {
    unsigned size = SIZE(HDRP(ptr));
    char* status = ALLOC(HDRP(ptr)) ? "Allocated" : "Free";
    printf("| Block #%zu | Size: %u | Status: %s |\n",
	   counter++,
	   size,
	   status);
    ptr = NEXT_BLKP(ptr);
  }
  printf("| Block #%zu | Size: %u | Status: %s |\n",
	 counter,
	 0,
	 "Allocated");
}

void dump_blocks(void)
{
  /*
    The same as print_blocks, but all information
    is written to a file.
  */
  FILE* dmp = fopen("block_dmp", "wt");
  size_t counter = 0;
  char* ptr = first_block;
  while (SIZE(HDRP(ptr)))
  {
    unsigned size = SIZE(HDRP(ptr));
    char* status = ALLOC(HDRP(ptr)) ? "Allocated" : "Free";
    fprintf(dmp, "| Block #%zu | Size: %u | Status: %s |\n",
	   counter++,
	   size,
	   status);
    ptr = NEXT_BLKP(ptr);
  }
  fprintf(dmp, "| Block #%zu | Size: %u | Status: %s |\n",
	 counter,
	 0,
	 "Allocated");
  fclose(dmp);
}

void print_seg_lst(void)
{
  /* 
    Prints the contents of the segregated list
    to the console.
  */
  for (int i = 0; i < NUM_CLASSES; i++)
  {
    printf("Size class: %u\n", classes[i]);
    printf("-------------------------------------------------\n");
    char** ptr = seg_lst[i];
    for (; ptr && *ptr; ptr = (char**)*ptr)
    {
      printf("| Size: %u | Address: %p | Successor: %p |\n",
	     SIZE(HDRP(ptr)),
	     ptr,
	     *ptr);
    }
    if (ptr)
      printf("| Size: %u | Address: %p | Successor: %s |\n",
	     SIZE(HDRP(ptr)),
	     ptr,
	     "None");
    printf("\n");
  }
}

void dump_seg_lst(void)
{
  /*
    Prints the contents of the segregated list
    to a file.
  */
  FILE* dmp = fopen("seglist_dmp", "wt");
  for (int i = 0; i < NUM_CLASSES; i++)
  {
    fprintf(dmp, "Size class: %u\n", classes[i]);
    fprintf(dmp, "-------------------------------------------------\n");
    char** ptr = seg_lst[i];
    for (; ptr && *ptr; ptr = (char**)*ptr)
    {
      fprintf(dmp, "| Size: %u | Address: %p | Successor: %p |\n",
	     SIZE(HDRP(ptr)),
	     ptr,
	     *ptr);
    }
    if (ptr)
      fprintf(dmp, "| Size: %u | Address: %p | Successor: %s |\n",
	     SIZE(HDRP(ptr)),
	     ptr,
	     "None");
    fprintf(dmp, "\n");
  }
  fclose(dmp);
}
