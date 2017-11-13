/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

#define WSIZE   4   /* word size (bytes) */
#define DSIZE   8   /* doubleword size (bytes) */
#define CHUNKSIZE  (1<<13)  
#define OVERHEAD  8   /* overhead of header and footer (bytes) */

/* choose a mode */
#define IMPLICIT_LIST 0
#define EXPLICIT_LIST 0
#define DEBUG 1

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)     (*(size_t *)(p))
#define PUT(p, val)  (*(size_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)     ((char *)(bp) - WSIZE)
#define FTRP(bp)     ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* single word (4) or double word (8) alignment */
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (DSIZE-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

//static unsigned int extend_cnt = 0;
static char* freeptr;
static char* allocptr;

// CONVENTION: ptr to a free block points to header+4bytes just as unfreed block

/*
 * delete_node: delete a node from the free list
 *
 */

static void delete_node(char *brk)
{
  assert(brk != NULL);
  char * prev_addr = *brk;
  *(prev_addr) = brk + WSIZE;
}

static void add_node(char *brk)
{
  assert(brk != NULL);
  /* prev is not changed */
  /* next is brk */
  *(freeptr+WSIZE) = brk;
  /* brk's prev is old freeptr*/
  *brk = freeptr;
  /* brk's next is unknown*/
  *(brk+WSIZE) = (void*)-1;
  /* advance freeptr to brk */
  freeptr = brk;
}

/*
 * coalesce: merge free blocks
 *
 */
static void *coalesce(char *brk)
{
#if IMPLICIT_LIST == 1
  /* Is the previous block allocated?  */
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(brk))); 
  /* Is the next block allocated?  */
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(brk)));
  /* the size of the current block */
  size_t size = GET_SIZE(HDRP(brk));
  if(prev_alloc && next_alloc)
  {
    return brk;
  }
  else if(prev_alloc && !next_alloc)
  {
  /* size is total size of current and next block*/
    size += GET_SIZE(HDRP(NEXT_BLKP(brk)));    
  /* create header and footer */  
    PUT(HDRP(brk), PACK(size, 0));    
    PUT(FTRP(brk), PACK(size, 0));
    return (brk);
  }
  else if( !prev_alloc && next_alloc )
  {
    /* current + prev */
    size += GET_SIZE(HDRP(PREV_BLKP(brk)));
    PUT(HDRP(PREV_BLKP(brk)), PACK(size, 0));
    PUT(FTRP(brk), PACK(size, 0));
    /* return previous block ptr as the current one is merged*/
    return (PREV_BLKP(brk));
  }
  else
  {
    /* both prev and next blocks are free */
    size += GET_SIZE(HDRP(PREV_BLKP(brk))) + GET_SIZE(FTRP(NEXT_BLKP(brk)));
    PUT(HDRP(PREV_BLKP(brk)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(brk)), PACK(size, 0));
    return (PREV_BLKP(brk));
  }
#endif
#if EXPLICIT_LIST == 1
  /* Is the previous block allocated?  */
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(brk))); 
  /* Is the next block allocated?  */
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(brk)));
  /* the size of the current block */
  size_t size = GET_SIZE(HDRP(brk));
  if(prev_alloc && next_alloc)
  {
    return brk;
  }
  else if(prev_alloc && !next_alloc)
  {
  /* size is total size of current and next block*/
    size += GET_SIZE(HDRP(NEXT_BLKP(brk)));
  /* create header and footer */
    PUT(HDRP(brk), PACK(size, 0));
    PUT(FTRP(brk), PACK(size, 0));
  /* delete next node from free list*/
    delete_node(NEXT_BLKP(brk));
    return (brk);
  }
  else if( !prev_alloc && next_alloc )
  {
    /* current + prev */
    size += GET_SIZE(HDRP(PREV_BLKP(brk)));
    PUT(HDRP(PREV_BLKP(brk)), PACK(size, 0));
    PUT(FTRP(brk), PACK(size, 0));
    /* delete current node from free list*/
    delete_node(brk);
    /* return previous block ptr as the current one is merged*/
    return (PREV_BLKP(brk));
  }
  else
  {
    /* both prev and next blocks are free */
    size += GET_SIZE(HDRP(PREV_BLKP(brk))) + GET_SIZE(FTRP(NEXT_BLKP(brk)));
    PUT(HDRP(PREV_BLKP(brk)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(brk)), PACK(size, 0));
    /* delete */
    delete_node(NEXT_BLKP(brk));
    delete_node(brk);

    return (PREV_BLKP(brk));
  }
#endif
#if DEBUG == 1
  /* Is the previous block allocated?  */
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(brk))); 
  /* Is the next block allocated?  */
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(brk)));
  /* the size of the current block */
  size_t size = GET_SIZE(HDRP(brk));
printf("prev_alloc=%d, next_alloc=%d\n", prev_alloc, next_alloc);
  if(prev_alloc && next_alloc)
  {
    printf("No merging happened, return brk = %p\n", brk);
    return brk;
  }
  else if(prev_alloc && !next_alloc)
  {
  /* size is total size of current and next block*/
    size += GET_SIZE(HDRP(NEXT_BLKP(brk)));
  /* create header and footer */
    PUT(HDRP(brk), PACK(size, 0));
    PUT(FTRP(brk), PACK(size, 0));
    return (brk);
  }
  else if( !prev_alloc && next_alloc )
  {
    /* current + prev */
    size += GET_SIZE(HDRP(PREV_BLKP(brk)));
    PUT(HDRP(PREV_BLKP(brk)), PACK(size, 0));
    PUT(FTRP(brk), PACK(size, 0));
      /* return previous block ptr as the current one is merged*/
    return (PREV_BLKP(brk));
  }
  else
  {
    /* both prev and next blocks are free */
    size += GET_SIZE(HDRP(PREV_BLKP(brk))) + GET_SIZE(FTRP(NEXT_BLKP(brk)));
    PUT(HDRP(PREV_BLKP(brk)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(brk)), PACK(size, 0));
    return (PREV_BLKP(brk));
  }
#endif

}

/*
 * extend_heap: extend the heap by calling mem_sbrk
 */
static char *extend_heap(size_t words)
{
#if IMPLICIT_LIST == 1
  char *oldbrk;
  size_t newsize = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
  if((int)(oldbrk = mem_sbrk(newsize)) == (void*)-1 )
  {
    return NULL;
  }
  PUT(HDRP(oldbrk), PACK(newsize, 0));	/* the new free block header */
  PUT(FTRP(oldbrk), PACK(newsize, 0));  /* free block footer */
  PUT(HDRP(NEXT_BLKP(oldbrk)), PACK(0, 1)); /* add eqilogue */
  /* coalesce if the previous block was free */
  return coalesce(oldbrk);
#endif
#if EXPLICIT_LIST == 1
  char *oldbrk;
  size_t newsize = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
  if((int)(oldbrk = mem_sbrk(newsize)) == (void*)-1 )
  {
    return NULL;
  }
  PUT(HDRP(oldbrk), PACK(newsize, 0));	/* the new free block header */
  PUT(FTRP(oldbrk), PACK(newsize, 0));  /* free block footer */
  PUT(HDRP(NEXT_BLKP(oldbrk)), PACK(0, 1)); /* add eqilogue */
  /* coalesce if the previous block was free */
  add_node(oldbrk);
  return coalesce(oldbrk);
#endif
#if DEBUG == 1
  char *oldbrk;
  size_t newsize = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
  if((int)(oldbrk = mem_sbrk(newsize)) == (void*)-1 )
  {
    printf("mem_sbrk in extend_heap failed at%p\n", oldbrk);
    return NULL;
  }

  /* Initialize free block header and footer and epilogue header*/
  PUT(HDRP(oldbrk), PACK(newsize, 0));	/* the new free block header */
  PUT(FTRP(oldbrk), PACK(newsize, 0));  /* free block footer */
  PUT(HDRP(NEXT_BLKP(oldbrk)), PACK(0, 1)); /* add eqilogue */
  printf("extended heap with old brk %p \n", oldbrk);

  /* coalesce if the previous block was free */
  return coalesce(oldbrk);


#endif
}


static char *find_fit(size_t newsize)
{
  #if IMPLICIT_LIST == 1
	char *brk;

	for(brk = mem_heap_lo() + DSIZE; GET_SIZE(HDRP(brk)) > 0; brk = NEXT_BLKP(brk))
	{
	  if(!GET_ALLOC(HDRP(brk)) && (newsize <= GET_SIZE(HDRP(brk))))
	    return brk;
	}
	return NULL;
  #endif
  #if EXPLICIT_LIST == 1
	char *brk;

	for(brk = freeptr; *(brk) != (void*)-1; brk = *(brk))
	{
	  if(newsize <= GET_SIZE(HDRP(brk)))
	    return brk;
	}
	return NULL;
  #endif
  #if DEBUG == 1
	char *brk;

	for(brk = mem_heap_lo() + DSIZE; GET_SIZE(HDRP(brk)) > 0; brk = NEXT_BLKP(brk))
	{
	  if(!GET_ALLOC(HDRP(brk)) && (newsize <= GET_SIZE(HDRP(brk))))
	    return brk;
	  printf("Searching blocks at %p\n", brk);
	}
	return NULL;
  #endif

}



static void place(void *brk, size_t asize)
{
  /* csize: free block size*/
  size_t csize = GET_SIZE(HDRP(brk));
 
  if((csize - asize) >= (DSIZE + OVERHEAD))
  {
    /* free block is more than double word larger than asize so we split the block*/
    /* header and footer*/
    PUT(HDRP(brk), PACK(asize, 1));
    PUT(FTRP(brk), PACK(asize, 1));
    brk = NEXT_BLKP(brk);
    /* split: put rest free space in the block into headers and footers*/
    PUT(HDRP(brk), PACK(csize-asize, 0));
    PUT(FTRP(brk), PACK(csize-asize, 0));
  }else
  {
    /* just a little bit larger. so we treat the tiny waste as a padding */
    PUT(HDRP(brk), PACK(csize, 1));
    PUT(FTRP(brk), PACK(csize, 1));
    brk += csize;
  }
}


/*
 * mm_init - initialize the malloc package.
 * 	Allocating initial heap area, 64M at beginning 	
 * 	return -1 if there is a problem, 0 otherwise
 */
int mm_init(void)
{
#if DEBUG == 1
  // allocate 2 megabyte
  allocptr = mem_sbrk(2 *(1<<20));
  printf("init allocptr %p\n", allocptr);
  //  max heap is 20M
  PUT(allocptr, 0);
  // create prologue
  PUT(allocptr+WSIZE, PACK(OVERHEAD, 1));
  PUT(allocptr+DSIZE, PACK(OVERHEAD, 1));
  allocptr += DSIZE;
//  freeptr = allocptr; // free is not implemented
// need implement a macro IS_FREE
  // create epilogue header
  PUT(allocptr+WSIZE, PACK(0, 1)); 
//  extend_heap(CHUNKSIZE/WSIZE);
  return 0;
#endif
#if EXPLICIT_LIST == 1
  allocptr = mem_sbrk(4 * WSIZE);
  PUT(allocptr, 0);
  // create prologue
  PUT(allocptr+WSIZE, PACK(OVERHEAD, 1));
  PUT(allocptr+DSIZE, PACK(OVERHEAD, 1));
  allocptr += DSIZE;
// freeptr = allocptr; // free is not implemented
// need implement a macro IS_FREE
  // create epilogue header
  PUT(allocptr+WSIZE, PACK(0, 1));  
//  if(extend_heap(CHUNKSIZE/WSIZE) == NULL )
//    return -1;
  return 0;
#endif 

#if IMPLICIT_LIST == 1
  allocptr = mem_sbrk(4 * WSIZE);
  PUT(allocptr, 0);
  // create prologue
  PUT(allocptr+WSIZE, PACK(OVERHEAD, 1));
  PUT(allocptr+DSIZE, PACK(OVERHEAD, 1));
  allocptr += DSIZE;
// freeptr = allocptr; // free is not implemented
// need implement a macro IS_FREE
  // create epilogue header
  PUT(allocptr+WSIZE, PACK(0, 1));  
//  if(extend_heap(CHUNKSIZE/WSIZE) == NULL )
//    return -1;
  return 0;
#endif 

}

/*
 * mm_exit - treat memory leak
 * 	free all unfreed memory blocks
 */
int mm_exit(void)
{
  mem_reset_brk();
  mem_deinit();
  return 1;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 *     * maintain alignment
 *     * find the suitable place
 *     * place the block
 *     * extend the heap if not enough space
 */
void *mm_malloc(size_t size)
{
#ifdef NAIVE
  size_t newsize = ALIGN(size + DSIZE);
  
  
  void * brk = mem_sbrk(newsize);
  if( brk == (void * )-1)
    return NULL;
  else
  {
  *(size_t *)brk = size;
  return (void*)((char *)brk + SIZE_T_SIZE);
  }
#endif
#if DEBUG == 1
  assert( size > 0);
  // maintain alignment
  // new size according to double word alignment
  // we add a DSIZE as header and footer is needed
  size_t newsize = ALIGN(size + DSIZE);
  size_t extendsize;

  // do i need to extend?
  if( (allocptr = find_fit(newsize)) != NULL) 
  {
    printf("\nfound a fit block%p\n", allocptr);
    place(allocptr, newsize);
    printf("after place function\n");
    return allocptr;
  }
  else 
    printf("\n\nfind fit failed\n");
  
  // every time heap is used up, we extend it by CHUNKSIZE or required block size
  extendsize = MAX(newsize, CHUNKSIZE);
  if((allocptr = extend_heap(extendsize/WSIZE)) == NULL)
  {
    printf("Extend heap failed\n");
    return NULL;
  }
  printf("after extend_heap, allocptr = %p\n", allocptr);
  printf("place a %d sized block at allocptr\n", newsize);
  place(allocptr, newsize);
  return allocptr;

#endif
#if EXPLICIT_LIST == 1
  assert( size > 0);
  // maintain alignment
  // new size according to double word alignment
  // we add a DSIZE as header and footer is needed
  size_t newsize = ALIGN(size + DSIZE);
  size_t extendsize;

  // do i need to extend?
  if( (allocptr = find_fit(newsize)) != NULL) 
  {
    place(allocptr, newsize);
    return allocptr;
  }
  
  // every time heap is used up, we extend it by CHUNKSIZE or required block size
  extendsize = MAX(newsize, CHUNKSIZE);
  if((allocptr = extend_heap(extendsize/WSIZE)) == NULL)
  {
    return NULL;
  }
  place(allocptr, newsize);
  return allocptr;
#endif


#if IMPLICIT_LIST == 1
  assert( size > 0);
  // maintain alignment
  // new size according to double word alignment
  // we add a DSIZE as header and footer is needed
  size_t newsize = ALIGN(size + DSIZE);
  size_t extendsize;

  // do i need to extend?
  if( (allocptr = find_fit(newsize)) != NULL) 
  {
    place(allocptr, newsize);
    return allocptr;
  }
  
  // every time heap is used up, we extend it by CHUNKSIZE or required block size
  extendsize = MAX(newsize, CHUNKSIZE);
  if((allocptr = extend_heap(extendsize/WSIZE)) == NULL)
  {
    return NULL;
  }
  place(allocptr, newsize);
  return allocptr;
#endif

}


/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{

#if IMPLICIT_LIST == 1
  size_t size = GET_SIZE(HDRP(ptr));

  PUT(HDRP(ptr), PACK(size, 0));
  PUT(FTRP(ptr), PACK(size, 0));
  coalesce(ptr);
#endif
#if DEBUG == 1
  printf("free is called\n");
  size_t size = GET_SIZE(HDRP(ptr));

  PUT(HDRP(ptr), PACK(size, 0));
  PUT(FTRP(ptr), PACK(size, 0));
  coalesce(ptr);
#endif
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
  void *oldptr = ptr;
  void *newptr;
  size_t copySize;

  newptr = mm_malloc(size);
  if (newptr == NULL)
    return NULL;
  copySize = *(size_t *)((char *)oldptr - WSIZE);
//  printf("caculated copySize = %d, while size = %d\n", copySize, size);
  if (size < copySize)
    copySize = size;    

  memcpy(newptr, oldptr, copySize);
//  printf("finished memcpy in realloc\n from oldptr =%p to newptr = %p\n", oldptr, newptr);
  mm_free(oldptr);
  return newptr;
}

/*
 * mm_check - Does not currently check anything
 * 	is every block marked as free?
 * 	is contiguous free blocks escaped coalescing?
 * 	is every free block in the free list?
 * 	do allocted blocks overlap?
 * 	do pointers in the heap block point to valid heap address?
 *	return nonzero if heap is consistent
 */
static int mm_check(void)
{
  return 1;
}

