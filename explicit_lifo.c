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
#include <sys/mman.h>
#include <errno.h>
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
// rounds up to the nearest multiple of alignment
#define	WSIZE		4 //�� ���� ũ��
#define	DSIZE		8 //���� ���� ũ��
#define CHUNKSIZE (1<<12) // 2**12 = (2**3)**4�̶� �׻� 8�� ����̴�
#define MAX(x, y) ((x) > (y)? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))
#define GET(p)		(*(unsigned int *)(p)) //����Ű�� ���� ���� �о�´�
#define PUT(p, val)	(*(unsigned int *)(p) = (val)) //�ַ� p�� �� val�� �����Ѵ�.
#define GET_SIZE(p)	(GET(p) & ~0x7) //�ּ� p�� �ִ� ũ�⸦ �о�´�.
#define GET_ALLOC(p)	(GET(p) & 0x1) //�ּ� p�� �ִ� allocated bit�� �о�´�.
#define HDRP(bp)	((char *)(bp) - WSIZE)
#define FTRP(bp)	((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp)	((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp)	((char *)(bp) - GET_SIZE(HDRP(bp) - WSIZE))
#define SUCC_FREEP(bp)  (*(void**)(bp + WSIZE))     // *(GET(SUCC_FREEP(bp))) == ���� ���븮��Ʈ�� bp //successor
#define PREC_FREEP(bp)  (*(void**)(bp))          // *(GET(PREC_FREEP(bp))) == ���� ���븮��Ʈ�� bp //Predecessor
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void*bp, size_t asize);
void removeBlock(void *bp);
void putFreeBlock(void *bp);
static char *heap_listp = NULL;
static char *free_listp = NULL; // ù��° ������� �����ͷ� ���
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	heap_listp = mem_sbrk(24); //���� �ּ�ũ�� �Ҵ�޾Ƽ� ù��° �ּҰ� ��ȯ
    if (heap_listp == (void*)-1)
    {
        return -1;
    }
    PUT(heap_listp, 0); //�е�
    PUT(heap_listp + WSIZE, PACK(16, 1)); //���ѷα� ���
    PUT(heap_listp + 2*WSIZE, NULL); //���ѷα� PREC ������ NULL�� �ʱ�ȭ
    PUT(heap_listp + 3*WSIZE, NULL); //���ѷα� SUCC ������ NULL�� �ʱ�ȭ
    PUT(heap_listp + 4*WSIZE, PACK(16, 1)); //���ѷα� ǲ��
    PUT(heap_listp + 5*WSIZE, PACK(0, 1)); //���ʷα� ���
    // heap_listp�� ���ѷα� ��� ����ĭ�� ��ġ��Ų��
    heap_listp += DSIZE;
    free_listp = heap_listp; // free_listp �ʱ�ȭ
    if (extend_heap(CHUNKSIZE/DSIZE) == NULL)
    {
        return -1;
    }
    // �Ҵ��� �Ǹ� 0�� ��ȯ �ȵǸ� -1�� ��ȯ�Ѵ�
    return 0;
}
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
static void *extend_heap(size_t words)
{
	size_t size;
    char * bp;
    size = words * DSIZE;
    if ((int)(bp = mem_sbrk(size)) == -1) // 1.bp�� long������ �ٲ��ִ� ����, 2.(long)�� (void*)�� ���� �� �ִ���.. 
    {   
        return NULL; // �Ҵ���� �� ���� �� NULL�� ��ȯ
    }
    // ���� ���� ������� ����� ǲ�͸� ������ְ� size�� 0�� �������ְ� ���� ���� ���(���ο� ���ʷα� ���)�� size 0�� alloc 1�� �������ش�
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    // �� �ڿ� ���� ���� ������ �������ְ� �����Ѵ�
    return coalesce(bp);
}
void *mm_malloc(size_t size)
{
    void* bp;
    size_t asize;
    if (size == 0)
    {
        return NULL;
    }
    // ���� size�� 8���� ������ �ּһ������� 16�� �Ǿ���Ѵ�.( 4 + 4 + 8)
    if (size <= DSIZE)
    {
        asize = 2 * DSIZE;
    }
    // 8���� ũ�ٸ� �ø��ؼ� 8�� ����� �Ǵ� �� �� �ּҰ��� �Ǿ���Ѵ�.
    else
    {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE); //size���� ū 8�� ��� �� �ּҰ�
    }
    // asize���� �� �� �ִ� ���� ������ ã�´�. find_fit�� bp�� ��ȯ�Ѵ�
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }
    else
    {
        size_t extend_size;
        extend_size = MAX(asize, CHUNKSIZE);
        bp = extend_heap(extend_size/DSIZE);
        if (bp == NULL)
        {
            return NULL;
        }
        place(bp, asize);
        return bp;
    }
    // ��������� �Ҵ�������� �ٲ��ش�. place
    // ���� ���� ������ ������ extendsize(asize, CHUNKSIZE)��ŭ�� ������ �Ҵ�ް� �Ҵ���� ���� ���� ���ʿ� place �Ѵ�.
}
static void *find_fit(size_t asize)
{
    // first fit
    void * bp;
    bp = free_listp;
    for (bp; !GET_ALLOC(HDRP(bp)); bp = SUCC_FREEP(bp))
    {
        if (GET_SIZE(HDRP(bp)) >= asize)
        {
            return bp;
        }
    }
    return NULL;
}
static void place(void*bp, size_t asize)
{
    size_t csize;
    csize = GET_SIZE(HDRP(bp));
    // ���븮��Ʈ���� ���ֱ�
    removeBlock(bp);
    if (csize - asize >= 16)
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        // ���븮��Ʈ ù��°�� ���ҵ� ���� �ִ´� coalesce()
        putFreeBlock(bp);
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}
void putFreeBlock(void *bp)
{
    SUCC_FREEP(bp) = free_listp;
    PREC_FREEP(bp) = NULL;
    PREC_FREEP(free_listp) = bp;
    free_listp = bp;
}
void removeBlock(void *bp)
{
    // ù��° ���� ���� �� // ���ѷα� ���� ���� ���� ����
    if (bp == free_listp)
    {
        PREC_FREEP(SUCC_FREEP(bp)) = NULL;
        free_listp = SUCC_FREEP(bp);
    }
    // �� �� ��� ���� ��
    else{
        SUCC_FREEP(PREC_FREEP(bp)) = SUCC_FREEP(bp);
        PREC_FREEP(SUCC_FREEP(bp)) = PREC_FREEP(bp);
    }
}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{   
    // bp�� ����� ����Ǿ��ִ� ����� ������ �ͼ�
    size_t size = GET_SIZE(HDRP(bp));
    // bp�� ����� ����Ǿ��ִ� ���� size, 0���� �ٲٰ�
    PUT(HDRP(bp), PACK(size, 0));
    // bp�� ǲ�Ϳ� ����Ǿ��ִ� ���� size, 0���� �ٲ��ְ�
    PUT(FTRP(bp), PACK(size, 0));
    // �� �ڿ� ���� ����Ʈ�� ���� ��찡 ������ coalesce�� ���ش�.
    coalesce(bp);
}
static void *coalesce(void *bp)
{
    // prev_alloc ���� ���� �ҵǾ��ִ��� Ȯ��
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    // next_alloc ���� ���� �Ҵ�Ǿ��ִ��� Ȯ��
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    // size bp ���� ������ ����
    size_t size = GET_SIZE(HDRP(bp));
    // ���� ��, ���� ���� ��� �Ҵ�Ǿ����� ��
    if (prev_alloc && next_alloc){}
    // ���� ���� ���� ���̰� ���� ���� �Ҵ�Ǿ����� ��
    else if (!prev_alloc && next_alloc)
    {
        removeBlock(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    // ���� ���� �Ҵ�Ǿ��ְ� ���� ���� ������� ��
    else if (prev_alloc && !next_alloc)
    {
        removeBlock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    // ���� ���� ��� ���� ���� ��
    else
    {
        removeBlock(PREV_BLKP(bp));
        removeBlock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))) + GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    putFreeBlock(bp);
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