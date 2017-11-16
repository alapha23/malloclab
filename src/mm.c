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
#define EXPLICIT_LIST 1
#define DEBUG 0

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
static char* freeptr = NULL;
static char* allocptr;
static int node_cnt = 0;

// CONVENTION: ptr to a free block points to header+4bytes just as unfreed block

/*
 * delete_node: delete a node from the free list
 * add_node: add a node to the free list
 */
#if DEBUG == 1
static void p_node(char *ptr)
{  
  printf("	 at pointer %p, prev_addr = %x, next_ptr=%x\n", ptr, *((size_t *)ptr),  *((size_t *)(ptr+WSIZE)));  
  fflush(stdout);
//  printf("freeptr at pointer%p, prev_addr=%x, next_ptr=%x\n", freeptr, *(size_t *)freeptr, *(size_t *)(freeptr+WSIZE) );  
}
static void p_list(void)
{
    
  char* ptr = freeptr;
  if(freeptr==NULL)
  {
    printf("List is empty\n");
    return ;
  }

  printf("freeptr at pointer%p, prev_addr=%x, next_ptr=%x\n", freeptr, *(size_t *)freeptr, *(size_t *)(freeptr+WSIZE) );  
  fflush(stdout);
  
  printf("	Start: ");
  p_node(ptr);
  for(; *(size_t *)ptr != (void*)-1; )
  {
    ptr = *(size_t*)(ptr);    
    if(*(size_t *)(ptr+ WSIZE) ==(void*)-1)
    {
      p_node(ptr);
      printf("		Error: next addr shouldnot be -1 inside the list\n\n\n");      
      fflush(stdout);
      exit(0);
    }
    if(*(size_t *)ptr == *(size_t *)(ptr+WSIZE))
    {
      p_node(ptr);
      printf("Error, next=previous\n");
      fflush(stdout);
      exit(0);
    }
    p_node(ptr);
    }
  printf("	End: ");
  p_node(ptr);
}

static void delete_node(char *brk)
{
  printf("Delete_node:");
  p_list();
  node_cnt--;
//  printf("node_cnt = %d\n", node_cnt);  
  // if the previous addr is -1, it means we need to delete the first node in the list
  if(*(size_t *)(brk) == (void *)-1)
  {
  // prev = -1, start of the list
    if(*(size_t *)(brk+WSIZE) == (void *)-1)
    {
      printf("Free list is empty after deleting\n");
      freeptr = NULL;
      return ;
    }
    else
    {
//      printf("delete node at pointer %p, prev_addr = %x\n", brk, *((size_t *)brk) );
//      prev is -1, while next is not -1
      printf("%x\n", *(size_t *)(brk+WSIZE) );
      printf("Delete node:");
      p_node(brk);
      char * next_addr = *((size_t *)(brk+WSIZE));
      PUT(next_addr, (void *)-1);      
    }
  }
  else
  {
  // neither the end of list nor the start of list
      printf("%x\n", *(size_t *)(brk+WSIZE) );
      if(*(size_t *)(brk+WSIZE) == (void*)-1)
      {
      // if the end of list, namely freeptr         
        printf("Delete freeptr\n");
        freeptr = *(size_t *)(brk);
	PUT(freeptr+WSIZE, (void*)-1);	
	fflush(stdout);
	return ;
      }

      printf("Delete normal node:");
      p_node(brk);
      char * next_addr = *((size_t *)(brk+WSIZE));
      char * prev_addr = *(size_t *)brk;
//      printf("Put next_addr=%p at prev_addr's next field=%p \n", next_addr, (char *)(prev_addr+WSIZE));
//      in prev node, next addr is the next addr of current node
      
      PUT((prev_addr+WSIZE), next_addr);
      PUT(next_addr, prev_addr);
  }
  printf("After deletion, the list:");
  p_list();
}

static void add_node(char *brk)
{
  assert(brk != NULL);
  printf("add_node:");
  p_list();
  node_cnt++;  
  if(NULL == freeptr)
  {
    printf("Add node, new ptr at%p\n", brk);
    fflush(stdout);
    freeptr = brk;
    /* at the first time the prev is itself*/
    PUT(brk, (void *)-1);
    PUT(brk+WSIZE, (void *)-1);
  //  PUT((size_t *)(brk+WSIZE), 0xffffffff);
//    printf("After add_node, freeptr=%p, *freeptr=%x, freeptr+WSIZE=%x, *(freeptr+WSIZE)=%x\n", freeptr, *((size_t *)freeptr), freeptr+WSIZE, *(freeptr+WSIZE));
    p_list();
    fflush(stdout);
    return ;
  }
  printf("In add node function we are supposed to free%p\n", brk);
  fflush(stdout);

  char * free_addr = freeptr; // which is also the old node
  char * new_node = brk;

  PUT(brk, free_addr);
  PUT(brk+WSIZE, (void*)-1);
  PUT(freeptr+WSIZE, new_node);
  printf("At the old node the next addr is %p\n", *(size_t*)(freeptr+WSIZE));
  freeptr = brk;
  printf("At the old node the next addr is %p\n", *(size_t *)(*(size_t*)(freeptr)+WSIZE));
  printf("Inside add_node");
    p_list();
    fflush(stdout);
  if(*(size_t *)(freeptr+WSIZE) != (void*)-1)
  {
    printf("Error *(size_t *)(freeptr+WSIZE) != (void*)-1\n\n\n");
    fflush(stdout);
    exit(0);
  }
  if(*(size_t *)(freeptr) != free_addr)
  {
    printf("Error: *(size_t *)(freeptr) != free_addr\n\n\n");
    fflush(stdout);
    exit(0);
  }
  
  printf("After add_node");
    p_list();
    fflush(stdout);
}
#endif

#if EXPLICIT_LIST == 1

static void delete_node(char *brk)
{
  node_cnt--;
  if(*(size_t *)(brk) == (void *)-1)
  {
    if(*(size_t *)(brk+WSIZE) == (void *)-1)
    {
      freeptr = NULL;
      return ;
    }
    else
    {
      char * next_addr = *((size_t *)(brk+WSIZE));
      PUT(next_addr, (void *)-1);      
    }
  }
  else
  {
  // neither the end of list nor the start of list
      if(*(size_t *)(brk+WSIZE) == (void*)-1)
      {
      // if the end of list, namely freeptr         
        freeptr = *(size_t *)(brk);
	PUT(freeptr+WSIZE, (void*)-1);	
	return ;
      }
      char * next_addr = *((size_t *)(brk+WSIZE));
      char * prev_addr = *(size_t *)brk;
      
      PUT((prev_addr+WSIZE), next_addr);
      PUT(next_addr, prev_addr);
  }
}

static void add_node(char *brk)
{
  assert(brk != NULL);
  node_cnt++;  
  if(NULL == freeptr)
  {
    freeptr = brk;
    /* at the first time the prev is itself*/
    PUT(brk, (void *)-1);
    PUT(brk+WSIZE, (void *)-1);
    return ;
  }

  char * free_addr = freeptr; // which is also the old node
  char * new_node = brk;

  PUT(brk, free_addr);
  PUT(brk+WSIZE, (void*)-1);
  PUT(freeptr+WSIZE, new_node);
  freeptr = brk;
}
#endif

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
  /* delete next node from free list*/
    delete_node(NEXT_BLKP(brk));
  /* size is total size of current and next block*/
    size += GET_SIZE(HDRP(NEXT_BLKP(brk)));
  /* create header and footer */
    PUT(HDRP(brk), PACK(size, 0));
    PUT(FTRP(brk), PACK(size, 0));
    return (brk);
  }
  else if( !prev_alloc && next_alloc )
  {
    // list became messed up before this position
    /* delete current node from free list*/
    delete_node(brk);
    /* current + prev */
    size += GET_SIZE(HDRP(PREV_BLKP(brk)));
    PUT(HDRP(PREV_BLKP(brk)), PACK(size, 0));
    PUT(FTRP(brk), PACK(size, 0));
    /* return previous block ptr as the current one is merged*/
    return (PREV_BLKP(brk));
  }
  else
  {
  /* delete */
    delete_node(NEXT_BLKP(brk));
    delete_node(brk);
    /* both prev and next blocks are free */
    size += GET_SIZE(HDRP(PREV_BLKP(brk))) + GET_SIZE(FTRP(NEXT_BLKP(brk)));
    PUT(HDRP(PREV_BLKP(brk)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(brk)), PACK(size, 0));
    return (PREV_BLKP(brk));
  }
#endif
#if DEBUG == 1
    printf("In coalesce function\n");
    fflush(stdout);
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
    printf(" delete next node from free list\n");
  /* delete next node from free list*/
    delete_node(NEXT_BLKP(brk));
  /* size is total size of current and next block*/
    size += GET_SIZE(HDRP(NEXT_BLKP(brk)));
  /* create header and footer */
    PUT(HDRP(brk), PACK(size, 0));
    PUT(FTRP(brk), PACK(size, 0));
    return (brk);
  }
  else if( !prev_alloc && next_alloc )
  {
    printf(" delete current node%p from free list\n", brk);
    // list became messed up before this position
    /* delete current node from free list*/
    delete_node(brk);
    /* current + prev */
    size += GET_SIZE(HDRP(PREV_BLKP(brk)));
    PUT(HDRP(PREV_BLKP(brk)), PACK(size, 0));
    PUT(FTRP(brk), PACK(size, 0));
    /* return previous block ptr as the current one is merged*/
    return (PREV_BLKP(brk));
  }
  else
  {
  /* delete */
    delete_node(NEXT_BLKP(brk));
    delete_node(brk);
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
  newsize += DSIZE;
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
  newsize += DSIZE;
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
  /* Search the freelist backwards from freeptr*/
        if(freeptr == NULL)
          return NULL;

	for(brk = freeptr; *(size_t *)(brk) != (void*)-1; )
	{
	  char * prev_ptr = *(size_t *)(brk);
	  if(newsize < GET_SIZE(HDRP(brk)))
	  {
	     return brk;
	  }	  
	  brk = prev_ptr;
	}

        if(newsize <= GET_SIZE(HDRP(brk)))
	  {
	     return brk;
        }
	return NULL;
  #endif
  #if DEBUG == 1
	char *brk;	
  /* Search the freelist backwards from freeptr*/
        if(freeptr == NULL)
          return NULL;

	printf("Starting to find fit from freeptr%p\n", freeptr);
	p_list();

	for(brk = freeptr; *(size_t *)(brk) != (void*)-1; )
	{
//	  printf("Number of free node=%d\n", node_cnt);
	  printf("Inside find_fit brk=%p, prev_=%x\n", brk, *(size_t *)(brk));
	  char * prev_ptr = *(size_t *)(brk);
	  printf("prev ptr = %p, prev_ptr's prev =%x \n", prev_ptr, *(size_t *)prev_ptr );
	  printf("Size of brk block=%d\n", GET_SIZE(HDRP(brk)));
	  fflush(stdout);
	  printf("Newsize = %d\n", newsize);
	  fflush(stdout);
	  if(newsize < GET_SIZE(HDRP(brk)))
	  {
	     printf("Found a block successfully\n");
	     fflush(stdout);
	     return brk;
	  }	  
	  brk = prev_ptr;
	}

        if(newsize <= GET_SIZE(HDRP(brk)))
	  {
	     printf("Found a block successfully\n");
	     return brk;
        }
	printf("failed to find a block\n");
	return NULL;
  #endif

}



static void place(void *brk, size_t asize)
{
#if EXPLICIT_LIST == 1
  size_t csize = GET_SIZE(HDRP(brk));
  if((csize - asize) > (DSIZE + 2* OVERHEAD))
  {
    /* free block is more than double word larger than asize so we split the block*/
    /* header and footer*/
    PUT(HDRP(brk), PACK(asize, 1));
    PUT(FTRP(brk), PACK(asize, 1));
    brk = NEXT_BLKP(brk);
    delete_node(PREV_BLKP(brk));

    /* split: put rest free space in the block into headers and footers*/
    PUT(HDRP(brk), PACK(csize-asize, 0));
    PUT(FTRP(brk), PACK(csize-asize, 0));
    add_node(brk);
  }else
  {
    /* just a little bit larger. so we treat the tiny waste as a padding */
    PUT(HDRP(brk), PACK(csize, 1));
    PUT(FTRP(brk), PACK(csize, 1));
    delete_node(brk);
    brk += csize;
  }

#endif

#if DEBUG == 1
  size_t csize = GET_SIZE(HDRP(brk));
  if((csize - asize) > (DSIZE + 2* OVERHEAD))
  {
    /* free block is more than double word larger than asize so we split the block*/
    /* header and footer*/
    PUT(HDRP(brk), PACK(asize, 1));
    PUT(FTRP(brk), PACK(asize, 1));
    brk = NEXT_BLKP(brk);
    delete_node(PREV_BLKP(brk));

    /* split: put rest free space in the block into headers and footers*/
    PUT(HDRP(brk), PACK(csize-asize, 0));
    PUT(FTRP(brk), PACK(csize-asize, 0));
    add_node(brk);
  }else
  {
    /* just a little bit larger. so we treat the tiny waste as a padding */
    PUT(HDRP(brk), PACK(csize, 1));
    PUT(FTRP(brk), PACK(csize, 1));
    delete_node(brk);
    brk += csize;
  }

#endif

#if IMPLICIT_LIST == 1
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
#endif
}


/*
 * mm_init - initialize the malloc package.
 * 	Allocating initial heap area, 64M at beginning 	
 * 	return -1 if there is a problem, 0 otherwise
 */
int mm_init(void)
{
#if DEBUG == 1
  mem_reset_brk();
  freeptr = NULL;  
  node_cnt = 0;
  printf("mm_init is called\n");
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
  if(extend_heap(CHUNKSIZE/WSIZE) == NULL )
    return -1;
  printf("mm_init is finished\n");
  return 0;

#endif
#if EXPLICIT_LIST == 1
  mem_reset_brk();
  freeptr = NULL;  
  node_cnt = 0;
  allocptr = mem_sbrk(4 * WSIZE);
  PUT(allocptr, 0);
  // create prologue
  PUT(allocptr+WSIZE, PACK(OVERHEAD, 1));
  PUT(allocptr+DSIZE, PACK(OVERHEAD, 1));
  allocptr += DSIZE;
  // create epilogue header
  PUT(allocptr+WSIZE, PACK(0, 1));  
  if(extend_heap(CHUNKSIZE/WSIZE) == NULL )
    return -1;
  return 0;
#endif 

#if IMPLICIT_LIST == 1
  allocptr = mem_sbrk(4 * WSIZE);
  PUT(allocptr, 0);
  // create prologue
  PUT(allocptr+WSIZE, PACK(OVERHEAD, 1));
  PUT(allocptr+DSIZE, PACK(OVERHEAD, 1));
  allocptr += DSIZE;
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

printf("\n\nmm_malloc is called\n");
p_list();
  assert( size > 0);
  // maintain alignment
  // new size according to double word alignment
  // we add a DSIZE as header and footer is needed
  size_t newsize = ALIGN(size + DSIZE );
  size_t extendsize;


  // do i need to extend?
  printf("Checking if extend is needed in mm_malloc\n");
  if( (allocptr = find_fit(newsize)) != NULL) 
  {
    printf("No need for extending the heap. Place a new block\n");
    place(allocptr, newsize);
    printf("Place is finished, mm_malloc is finished\n");
    return allocptr;
  }else
    printf("Failed to find a fit free block\n");
    printf("Extending heap is needed\n");
  
  // every time heap is used up, we extend it by CHUNKSIZE or required block size
  extendsize = MAX(newsize, CHUNKSIZE);
  if((allocptr = extend_heap(extendsize/WSIZE)) == NULL)
  {
    return NULL;
  }  
  place(allocptr, newsize);
  printf("Heap extended, mm_malloc is finished\n");
  p_list();

  return allocptr;

#endif
#if EXPLICIT_LIST == 1
  assert( size > 0);
  // maintain alignment
  // new size according to double word alignment
  // we add a DSIZE as header and footer is needed
  size_t newsize = ALIGN(size + DSIZE );
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
  printf("\n\nmm_free is called to freee%p\n", ptr);
  size_t size = GET_SIZE(HDRP(ptr));
  PUT(HDRP(ptr), PACK(size, 0));
  PUT(FTRP(ptr), PACK(size, 0));  
  add_node(ptr);
  coalesce(ptr);  
#endif
#if EXPLICIT_LIST == 1
  size_t size = GET_SIZE(HDRP(ptr));
  PUT(HDRP(ptr), PACK(size, 0));
  PUT(FTRP(ptr), PACK(size, 0));  
  add_node(ptr);
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
#if DEBUG == 1
static int mm_check(void)
{
  return 1;
}
#endif
