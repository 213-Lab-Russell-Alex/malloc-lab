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


//added signatures for helper funcs
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Team i hope this is over soon",
    /* First member's full name */
    "Alex Gist",
    /* First member's email address */
    "alexandergist2022@u.northwestern.edu",
    /* Second member's full name (leave blank if none) */
    "Russell MacQuarrie",
    /* Second member's email address (leave blank if none) */
    "russellmacquarrie2022@u.northwestern.edu"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

//my macros
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc)) //packs size and bit into word?dk how tho

#define GET(p) (* (unsigned int *)(p)) //reads and writes to heap
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7) //gets size/payload fields from pointer
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE) // header and footer addresses
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP_ALL(bp) ((char *)(bp) + GET_SIZE(((char*) (bp) - WSIZE)))
#define PREV_BLKP_ALL(bp) ((char *)(bp) - GET_SIZE(((char *) (bp) - DSIZE)))

#define NEXT_BLKP_FREE(bp) ((char *)(bp) + WSIZE)
#define PREV_BLKP_FREE(bp) ((char *)(bp))

//GLOBAL VARS
static char *heap_listp; //points to the prologue block

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3*WSIZE), PACK(0,1));
    heap_listp += (2*WSIZE);
    if(extend_heap(CHUNKSIZE/WSIZE) == NULL) return -1;
    
    PUT(heap_listp, NULL); //should put NULL into the first word, check size tho
    PUT(heap_listp + WSIZE, NULL) //setting "next" pointer
    
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;
    if (size==0) return NULL;
    if(size<=DSIZE) asize = 2*DSIZE; //set to 16 byte minimum if shorter
    else asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/DSIZE); //adding header/footer space
    
    if((bp = find_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }
    
    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL) return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
    
    prev_iterator = PREV_BLKP_ALL(ptr); //finding prev free block in order
    while (GET_ALLOC(HDRP(prev_iterator))){
        prev_iterator = PREV_BLKP_ALL(ptr);
    }
    PUT(PREV_BLKP_FREE(ptr), prev_iterator);
    
    next_iterator = NEXT_BLKP_ALL(ptr); //finding prev free block in order
    while (GET_ALLOC(HDRP(prev_iterator))){
        next_iterator = NEXT_BLKP_ALL(ptr);
    }
    PUT(NEXT_BLKP_FREE(ptr), next_iterator);
    
    
    
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
    copySize = size;
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}




static void *extend_heap(size_t words){
    char *bp;
    size_t size;
    size = (words % 2) ? (words+1)*WSIZE : words*WSIZE;
    
    if ((long)(bp = mem_sbrk(size)) == -1) return NULL;
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP_ALL(bp)), PACK(0,1));
    return coalesce(bp);
}

static void *coalesce(void *bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP_ALL(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP_ALL(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    if(prev_alloc && next_alloc) return bp;
    else if(prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP_ALL(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if(!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP_ALL(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP_ALL(bp)), PACK(size, 0));
        bp = PREV_BLKP_ALL(bp);
    }
    else{
        size += GET_SIZE(HDRP(PREV_BLKP_ALL(bp))) + GET_SIZE(FTRP(NEXT_BLKP_ALL(bp)));
        PUT(HDRP(PREV_BLKP_ALL(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP_ALL(bp)), PACK(size, 0));
        bp = PREV_BLKP_ALL(bp);
    }
    return bp;
}

static void *find_fit(size_t asize){
    void *bp;
    for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP_FREE(bp)){
        if(asize <= GET_SIZE(HDRP(bp))){
            return bp;
        }
    }
    return NULL;
}

static void place(void *bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));
    next_free = NEXT_BLKP_FREE(bp); //store the prev/next free blocks
    prev_free = PREV_BLKP_FREE(bp);
    
    if((csize-asize) >= (2*DSIZE)){ //if leftover bits can make own block (at least 16 bytes)
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        
        bp = NEXT_BLKP_ALL(bp); //goes to next leftover space
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        
        PUT(NEXT_BLKP_FREE(bp), next_free); //set prev/next of new block
        PUT(PREV_BLKP_FREE(bp), prev_free);
    }
    else{
        PUT(NEXT_BLKP_FREE(*(prev_free)), next_free); //connect remaining free list
        PUT(PREV_BLKP_FREE(*(next_free)), prev_free);
        
        
        PUT(HDRP(bp), PACK(csize, 1)); //set whole block to allocated
        PUT(FTRP(bp), PACK(csize, 1));
    }
}








