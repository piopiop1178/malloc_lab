//seg_llist 후입 선출 방식

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    /* Team name */
    "Kangsan",
    /* First member's full name */
    "Kangsan Kim",
    /* First member's email address */
    "kkk",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

//////Reviewer 김승현 

////// 1. Macro의 경우 필요한 것만 잘 정의하신 것 같습니다. Looks good to me

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
//블록 최소 크기인 2**4부터 최대 크기인 2**32를 위한 리스트
#define LIST_NUM 29

////// 필요한 기능만 함수로 잘 구현한 것 같습니다. LGTM
static void *extend_heap(size_t words);
void *mm_malloc(size_t size);
void mm_free(void *ptr);
static void *coalesce(void *bp);
void *mm_realloc(void *ptr, size_t size);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
void delete_block(char* bp);
void add_free_block(char* bp);
int get_seg_list_num(size_t size);


static char *heap_listp;
void* seg_list[LIST_NUM];


////// seg_list를 독립적인 array로 선언하였기 때문에 초기 word의 크기가 4인 것이 명확하게 이해됩니다.
int mm_init(void)
{   
    for (int i = 0; i < LIST_NUM; i ++){
        seg_list[i] = NULL;
    }

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

////// free block의 정보를 갱신해주고 coalesce에서 한 번에 처리하려는 의도가 보여 깔끔하다고 느껴집니다. LGTM

///// 만일 단순 구현이 아닌 점수 획득에 더 목표를 두신다면 이러한 생각도 해 볼 수 있습니다.
///// Malloc lab의 채점기에서 Util의 최대 점수는 60, Thru의 최대 점수는 40입니다.
///// 현재 이 코드의 점수는 Perf index = 43 (util) + 40 (thru) = 83/100 이므로 점수를 올리고 싶으시다면 
///// Util을 올려야 합니다. 이를 위한 Trick으로써 extend_heap함수를 호출할 때 extendsize = MAX(asize, CHUNKSIZE)으로 
///// words를 최소 CHUNKSIZE로 선언하지 않고 현재 할당하려고 하는 값에서 부족한 만큼만 늘려주는 방법이 있겠습니다.

///// 물론 이 방법은 고득점을 위한 방법이지 어떻게 input이 들어올 지 알 수 없는
///// 실제 환경에서는 다각적인 관점에서는 다른 관점으로 해석할 필요가 있습니다.

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

////// 요구 size를 Align해준 뒤 find해보고 없으면 extend 한다는 로직이 명확합니다. LGTM
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

////// 정보만 갱신해주고 coalesce에서 한 번에 처리해준다는 것이 깔끔합니다. LGTM
void mm_free(void *bp)
{   
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    coalesce(bp);
}

////// 함수화를 통해 중복되는 코드를 줄이고 논리를 명확히 한다는 점이 깔끔합니다. LGTM
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
    
    add_free_block(bp);
    return bp;
}

////// 아마 시간이 있으셨다면 이 부분을 더 개선하셨을 것 같습니다. 
////// 점수를 개선하기 위한 Trick으로는 단순하게 다른 가용 블럭에 요구 size만큼 복제하고 free시키는 방법이 아닌 
////// 현재 할당된 블럭의 뒷 영역이 가용 블럭이고, realloc에 필요한 크기 이상이라면 그 부분에 할당하는 방법도 있습니다.
////// 해당 방법을 사용하면 Util을 개선하여 점수를 올리실 수 있을 것입니다. 
////// 다만 이는 점수를 올리기 위한 Trick의 영역이고 실제 구현에서는 다각적 관점으로 생각해야 될 것 같습니다.
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

////// while문과 for문을 통해 각 seg_list를 순회하며 가용 블럭을 찾는다는 로직이 깔끔합니다. LGTM
static void *find_fit(size_t asize)
{  
    void *search_p;
    int i = get_seg_list_num(asize);
    while (i < LIST_NUM){
        for (search_p = seg_list[i]; search_p != NULL; search_p = GET(SUCC(search_p))){
            if (GET_SIZE(HDRP(search_p)) >= asize){
                return search_p;
            }
        }
        i ++;
    }

    return NULL;
}

////// block의 연결상태를 처리해주는 논리가 깔끔합니다. LGTM
///// 다만 old_size보다 더 직관적인 변수명이 있을 것이라는 생각이 듭니다.
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
    else {
        PUT(HDRP(bp), PACK(old_size, 1));
        PUT(FTRP(bp), PACK(old_size, 1));
    }
}

////// block의 연결상태를 처리해주는 논리가 깔끔합니다. LGTM
void delete_block(char* bp){ 
    int seg_list_num = get_seg_list_num(GET_SIZE(HDRP(bp)));


    if (GET(PRED(bp)) == NULL){
        if (GET(SUCC(bp)) == NULL){
            seg_list[seg_list_num] = NULL;
        } else {
            PUT(PRED(GET(SUCC(bp))), NULL);
            seg_list[seg_list_num] = GET(SUCC(bp));
        }
    } 
    else {
        if (GET(SUCC(bp)) == NULL){
            PUT(SUCC(GET(PRED(bp))), NULL);
        } else {
            PUT(PRED(GET(SUCC(bp))), GET(PRED(bp)));
            PUT(SUCC(GET(PRED(bp))), GET(SUCC(bp)));
        }
    }
}

////// block을 추가하기 위해 연결상태를 처리해주는 논리가 깔끔합니다. LGTM
void add_free_block(char* bp){
    int seg_list_num = get_seg_list_num(GET_SIZE(HDRP(bp)));
    if (seg_list[seg_list_num] == NULL){
        PUT(PRED(bp), NULL);
        PUT(SUCC(bp), NULL);
    } else {
        PUT(PRED(bp), NULL);
        PUT(SUCC(bp), seg_list[seg_list_num]);
        PUT(PRED(seg_list[seg_list_num]), bp);
    }
    seg_list[seg_list_num] = bp;
}

////// seg_list의 index를 받겠다는 것이 깔끔합니다. LGTM
int get_seg_list_num(size_t size){
    //seg_list[0]은 블록의 최소 크기인 2**4를 위한 리스트 
    int i = -4;
    while (size != 1){
        size = (size >> 1);
        i ++;
    }
    return i;
}

////// 전체적으로 잘 짠 코드인 것 같습니다. 
////// 특히 함수들의 명칭과 의도가 명확하고, 함수를 구현함에 있어서도 딱 필요한 논리대로만 깔끔하게 구현했다는 생각이 듭니다.
////// 가장 좋았던 부분은 explicit에서 segregated로의 로직 변경이 seg_list의 선언과 get_seg_list_num으로 인해 
////// 굉장히 깔끔하게 구현되었다는 점입니다.
////// 기회가 된다면 같은 팀으로 함께 해보고 싶습니다. 한 주 동안 고생 많으셨습니다!