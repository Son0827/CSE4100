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

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
    /* Your student ID */
    "20181085",
    /* Your full name*/
    "Chaehun Son",
    /* Your email address */
    "corb7@sogang.ac.kr",
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE 4 /* Word and header/footer size (bytes) */            // line:vm:mm:beginconst
#define DSIZE 8                                                      /* Doubleword size (bytes) */
#define CHUNKSIZE (1 << 12) /* Extend heap by this amount (bytes) */ // line:vm:mm:endconst
#define INITCHUNKSIZE (1 << 6)

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc)) // line:vm:mm:pack

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))              // line:vm:mm:get
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // line:vm:mm:put

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7) // line:vm:mm:getsize
#define GET_ALLOC(p) (GET(p) & 0x1) // line:vm:mm:getalloc

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp)-WSIZE)                        // line:vm:mm:hdrp
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // line:vm:mm:ftrp

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE))) // line:vm:mm:nextblkp
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))   // line:vm:mm:prevblkp
/* $end mallocmacros */

#define SUCP(bp) (*(void **)(bp))
#define PREP(bp) (*(void **)(bp + WSIZE))

#define REALLOC_BUFFER (1<<7)

static char *free_listp;
static char *heap_listp;
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void* place(void *bp, size_t asize);
static void insert(void *bp, size_t size);
static void delete(void *bp);


/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(6 * WSIZE)) == (void *)-1) // line:vm:mm:begininit
        return -1;
    PUT(heap_listp, 0);                                /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE * 2, 1)); /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), (int)NULL);
    PUT(heap_listp + (3 * WSIZE), (int)NULL);
    PUT(heap_listp + (4 * WSIZE), PACK(DSIZE * 2, 1)); /* Prologue footer */
    PUT(heap_listp + (5 * WSIZE), PACK(0, 1));         /* Epilogue header */
    free_listp = heap_listp + DSIZE;
    heap_listp += (3 * WSIZE);

    SUCP(free_listp) = NULL;
    PREP(free_listp) = NULL;

    if (extend_heap(INITCHUNKSIZE / WSIZE) == NULL)
        return -1;
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
    char *bp = NULL;

    if (size == 0)
        return NULL;

    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    if ((bp = find_fit(asize)) != NULL)
    {
        bp = place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    bp = place(bp, asize);
    
    return bp;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

static void insert(void *bp, size_t size)
{
    if (SUCP(free_listp) == NULL)
    {
        PREP(bp) = free_listp;
        SUCP(bp) = NULL;
        SUCP(free_listp) = bp;
        return;
    }
    else{
        SUCP(bp) = SUCP(free_listp);
        PREP(SUCP(free_listp)) = bp;
        SUCP(free_listp) = bp;
        PREP(bp) = free_listp;
    }

}

static void delete(void *bp)
{
    if (SUCP(bp) == NULL)
    {
        SUCP(PREP(bp)) = NULL;
    }
    else
    {
        SUCP(PREP(bp)) = SUCP(bp);
        PREP(SUCP(bp)) = PREP(bp);
    }
}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && !next_alloc)
    {
        delete(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc)
    {
        delete(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else if (!prev_alloc && !next_alloc)
    {
        delete(PREV_BLKP(bp));
        delete(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    insert(bp, size);

    return bp;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *new_ptr = ptr;
    size_t asize = size;

    if(size == 0)
        return NULL;

    if(asize <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);


    if(GET_SIZE(HDRP(ptr)) < asize){
        if(!GET_SIZE(HDRP(NEXT_BLKP(ptr)))){
            while(GET_SIZE(HDRP(ptr)) + GET_SIZE(HDRP(NEXT_BLKP(ptr))) <= asize){
            if(extend_heap(INITCHUNKSIZE/WSIZE) == NULL)
                return NULL;
            }
        }

        if(!GET_ALLOC(NEXT_BLKP(ptr)) && (GET_SIZE(HDRP(ptr))+GET_SIZE(HDRP(NEXT_BLKP(ptr))) >= asize)){
            delete(NEXT_BLKP(ptr));

            PUT(HDRP(ptr), PACK((GET_SIZE(HDRP(ptr))+GET_SIZE(HDRP(NEXT_BLKP(ptr)))), 1));
            PUT(FTRP(ptr), PACK((GET_SIZE(HDRP(ptr))+GET_SIZE(HDRP(NEXT_BLKP(ptr)))), 1));

        }
        else{
            new_ptr = mm_malloc(asize - DSIZE);
            memcpy(new_ptr, ptr, MIN(size,asize));
            mm_free(ptr);
        } 
    }
    
    return new_ptr;
}

static void* place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    size_t dif = csize - asize;
    
    delete(bp);

    if(dif < DSIZE * 2){
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        return bp;
    }
    else if ((asize >= 100))
    {
        PUT(HDRP(bp), PACK(dif, 0));
        PUT(FTRP(bp), PACK(dif, 0));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(asize, 1));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(asize, 1));
        coalesce(bp); 
        return NEXT_BLKP(bp);
    }
    else
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(dif, 0));
        PUT(FTRP(bp), PACK(dif, 0));
        coalesce(bp);
        return PREV_BLKP(bp);
    }
}

static void *find_fit(size_t asize)
{
    void *bp;
    void *best_fit = NULL;
    size_t min = 0;

    for (bp = SUCP(free_listp); bp != NULL; bp = SUCP(bp))
    {
        size_t dif = GET_SIZE(HDRP(bp)) - asize;
        
        if (asize <= GET_SIZE(HDRP(bp)))
        {
            if ((best_fit == NULL) || (dif < min))
            {
                best_fit = bp;
                min = dif;
            }
        }
    }

    return best_fit;
}
