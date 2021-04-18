//주소 순으로 관리하는 first fit 구현해보기 
//주소 순으로 관리 -> 가용블럭 연결 리스트 만들기 -> 주소가 작은게 앞으로 오도록 
//포인터 변수 = 8byte -> 가용블럭 최소 크기 = header, 포인터 2개, footer = 24byte

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Jungle",
    /* First member's full name */
    "Kangsan Kim",
    /* First member's email address */
    "piopiop1178@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
    
};

#define ALIGNMENT 8

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12) 

#define MAX(x, y) ((x) > (y)? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc)) 

#define GET(p)          (*(unsigned int *)(p)) 
#define PUT(p, val)     (*(unsigned int *)(p) = (val))  

#define GET_SIZE(p)     (GET(p) & ~0x7) 
#define GET_ALLOC(p)    (GET(p) & 0x1)

#define HDRP(bp)        ((char *)(bp) - WSIZE) // 
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) 

#define PRED(bp)        ((char *)(bp))
#define SUCC(bp)        ((char *)(bp) + WSIZE)

#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) 
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) 

static char *heap_listp;
static char *first_free_block; 

static void *extend_heap(size_t words);
void *mm_malloc(size_t size);
void mm_free(void *ptr);
static void *coalesce(void *bp);
void *mm_realloc(void *ptr, size_t size);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
void delete_block(char* bp);
void put_block(char* bp);
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{   first_free_block = NULL;
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) 
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) 
        return -1;
    return 0;
}

static void *extend_heap(size_t words)
{   
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * DSIZE : words * DSIZE; 

    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0)); 
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
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

    if (size == 0)
        return NULL;
    
    if (size <= DSIZE)
        asize = 2*DSIZE; 
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

void mm_free(void *bp)
{   
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    coalesce(bp);
}

static void *coalesce(void *bp)
{   
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); 
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        
        delete_block(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));

        delete_block(PREV_BLKP(bp));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else if (!prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))) + GET_SIZE(HDRP(PREV_BLKP(bp)));

        delete_block(PREV_BLKP(bp));
        delete_block(NEXT_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    
    put_block(bp);
    return bp;
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
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

static void *find_fit(size_t asize)
{  
    void *bp;
    for (bp = first_free_block; bp != NULL; bp = GET(SUCC(bp))){
        if (GET_SIZE(HDRP(bp)) >= asize){
            return bp;
        }
    }
    return NULL;
}

static void place(void *bp, size_t asize)
{   
    delete_block(bp);
    size_t old_size = GET_SIZE(HDRP(bp));
    if (old_size >= asize + (2 * DSIZE)){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        
        PUT(HDRP(NEXT_BLKP(bp)), PACK((old_size - asize), 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK((old_size - asize), 0));
        
        coalesce(NEXT_BLKP(bp));
    }
    //아니면 모든 블록 할당
    else {
        PUT(HDRP(bp), PACK(old_size, 1));
        PUT(FTRP(bp), PACK(old_size, 1));
    }
}

void delete_block(char* bp){ 
    if (GET(PRED(bp)) == NULL){
        if (GET(SUCC(bp)) == NULL){
            first_free_block = NULL;
        } else {
            first_free_block = GET(SUCC(bp));
            PUT(PRED(GET(SUCC(bp))), NULL);
        }
    } else {
        if (GET(SUCC(bp)) == NULL){
            PUT(SUCC(GET(PRED(bp))), NULL);
        }
        else {
            PUT(SUCC(GET(PRED(bp))), GET(SUCC(bp)));
            PUT(PRED(GET(SUCC(bp))), GET(PRED(bp)));
        }
    }
}

void put_block(char* bp){
    if (first_free_block == NULL){
        first_free_block = bp;
        PUT(PRED(bp), NULL);
        PUT(SUCC(bp), NULL);
    } else {
        char *list_p = first_free_block;
        for (list_p; list_p != NULL; list_p = GET(SUCC(list_p))){
            if (list_p > bp){
                if (list_p == first_free_block){
                    PUT(PRED(bp), NULL);
                    PUT(SUCC(bp), list_p);
                    PUT(PRED(list_p), bp);
                    first_free_block = bp;
                    break;
                } else {
                    PUT(SUCC(GET(PRED(list_p))), bp);
                    PUT(PRED(bp), GET(PRED(list_p)));
                    PUT(SUCC(bp), list_p);
                    PUT(PRED(list_p), bp);
                    break;
                }
            } 
            if (GET(SUCC(list_p)) == NULL){
                PUT(PRED(bp), list_p);
                PUT(SUCC(bp), NULL);
                PUT(SUCC(list_p), bp);
                break;
            }
        }
    }
}