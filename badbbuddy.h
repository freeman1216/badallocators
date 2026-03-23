// Simple bbuddy allocator implementation, uses an arena for its control block and
// an array of pointers to free blocks, free blocks are embedded in the underlying memory
// define BAD_BBUDDY_IMPLEMENTATION in one C file to use it
#pragma once
#ifndef BAD_BBUDDY
#define BAD_BBUDDY

#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef BAD_BBUDDY_DEF
#ifdef BAD_BBUDDY_STATIC
    #define BAD_BBUDDY_DEF static inline
#else
    #define BAD_BBUDDY_DEF extern
#endif
#endif

typedef struct bad_bbuddy_free_node {
    struct bad_bbuddy_free_node* next;
    struct bad_bbuddy_free_node* prev;
}bad_bbuddy_free_node_t;

typedef struct {
    uintptr_t base;
    size_t heads_bmask;
    size_t min_order;
    size_t max_order;
    size_t size;
    size_t free_list_size;
    bad_bbuddy_free_node_t *list;
    size_t *bmask;
    size_t bmask_size;
    bool owns_memory;
}bad_bbuddy_t;


#define BAD_BBUDDY_ALIGN (_Alignof(bad_bbuddy_t))
#define BAD_BBUDDY_ALIGN_ATTR __attribute__((aligned(BAD_BBUDDY_ALIGN)))

 #define BAD_BBUDDY_BMASK_SIZE(max_order,min_order)({\
    _Static_assert(max_order > min_order,"Max order should be bigger then min order");\
    _Static_assert((size_t)1 << (min_order) >= \
            sizeof(bad_bbuddy_free_node_t),"min_order should be at least log2(sizeof(node)");\
    ((((1 << (max_order - min_order))-1) + (sizeof(size_t)*8) - 1) /(sizeof(size_t)*8));\
})

#define BAD_BBUDDY_FREELIST_SIZE(max_order,min_order)({\
    _Static_assert(max_order > min_order,"Max order should be bigger then min order");\
    _Static_assert((size_t)1 << (min_order) >= \
            sizeof(bad_bbuddy_free_node_t),"min_order should be at least log2(sizeof(node)");\
    (((max_order) - (min_order) +1)  * (sizeof(bad_bbuddy_free_node_t))); \
})

#define BAD_BBUDDY_ALLOC_SIZE(max_order,min_order) ({ \
    _Static_assert(max_order > min_order,"Max order should be bigger then min order");\
    _Static_assert((size_t)1 << (min_order) >= \
            sizeof(bad_bbuddy_free_node_t),"min_order should be at least log2(sizeof(ptr)");\
    (sizeof(bad_bbuddy_t) + BAD_BBUDDY_FREELIST_SIZE((max_order),(min_order))\
     + BAD_BBUDDY_BMASK_SIZE((max_order),(min_order))); \
})

#define BAD_BBUDDY_GET_FREELIST_PTR(name)({\
    _Static_assert(sizeof(*(name)) == 1,"Macro requires byte ptr");\
    ((name)+sizeof(bad_bbuddy_t)); \
})
#define BAD_BBUDDY_GET_FREELIST_END(name,max_order,min_order)({\
    _Static_assert(sizeof(*(name)) == 1,"Macro requires byte ptr");\
    (BAD_BBUDDY_GET_FREELIST_PTR((name)) + (BAD_BBUDDY_FREELIST_SIZE((max_order),(min_order)) * \
                                          sizeof(bad_bbuddy_free_node_t))); \
})

#define BAD_BBUDDY_GET_BMASK_PTR(name,max_order,min_order)({\
    _Static_assert(sizeof(*(name)) == 1,"Macro requires byte ptr");\
    (BAD_BBUDDY_GET_FREELIST_END((name),(max_order),(min_order))); \
})

#define BAD_BBUDDY_GET_BMASK_END(name,max_order,min_order)({\
    _Static_assert(sizeof(*(name)) == 1,"Macro requires byte ptr");\
    (BAD_BBUDDY_GET_BMASK_PTR(name,max_order,min_order) + (BAD_BBUDDY_BMASK_SIZE((max_order),(min_order)) * \
                                      sizeof(size_t))); \
})

#define BAD_BBUDDY_ALLOCATE_STATIC(name,max_order,min_order)\
    uint8_t BAD_BBUDDY_ALIGN_ATTR name##_bbuddy_mem[BAD_BBUDDY_ALLOC_SIZE(max_order,min_order)+\
                                                    ((size_t)1 << (max_order))];\
    bad_bbuddy_t *name = bad_bbuddy_init(name##_bbuddy_mem,BAD_BBUDDY_GET_FREELIST_PTR(name##_bbuddy_mem),\
            BAD_BBUDDY_GET_BMASK_PTR(name##_bbuddy_mem,(max_order),(min_order)),\
            BAD_BBUDDY_GET_BMASK_END(name##_bbuddy_mem,(max_order),(min_order)),(max_order),(min_order))

BAD_BBUDDY_DEF bad_bbuddy_t* bad_bbuddy_init(
        void *bbuddy_mem,
        void *freelist_mem,
        void *bmask_mem,
        void *mem,
        size_t max_order,
        size_t min_order);
BAD_BBUDDY_DEF bad_bbuddy_t* bad_bbuddy_init_allocate(
        void *bbuddy_mem, 
        void *freelist_mem,
        void *bmask,
        size_t max_order,
        size_t min_order);
BAD_BBUDDY_DEF void* bad_bbuddy_alloc(bad_bbuddy_t *bbuddy,size_t size);
BAD_BBUDDY_DEF void bad_bbuddy_free(bad_bbuddy_t *bbuddy,void *block, size_t size);
BAD_BBUDDY_DEF void bad_bbuddy_deinit(bad_bbuddy_t *bbuddy);

#ifdef BAD_BBUDDY_IMPLEMENTATION

BAD_BBUDDY_DEF bad_bbuddy_t* bad_bbuddy_init(
        void *bbuddy_mem,
        void *freelist_mem,
        void *bmask_mem,
        void *mem,
        size_t max_order,
        size_t min_order)
{
    bad_bbuddy_t *bbuddy = bbuddy_mem;
    bbuddy->min_order = min_order;
    bbuddy->max_order = max_order;
    if(max_order < min_order ){
        return NULL;
    }

    size_t free_list_size = max_order - min_order + 1;
    size_t size_of_allocation = (size_t)1<<max_order;  
    bbuddy->list = freelist_mem; 
    bbuddy->base = (uintptr_t)mem;
    bbuddy->bmask_size = (((1 << (max_order - min_order))-1) + (sizeof(size_t)*8) - 1) /(sizeof(size_t)*8);
    bbuddy->list[0].next = (bad_bbuddy_free_node_t *)bbuddy->base;
    bbuddy->list[0].prev =  (bad_bbuddy_free_node_t *)bbuddy->base;
    bad_bbuddy_free_node_t * embedded_node = (bad_bbuddy_free_node_t *)bbuddy->base;
    embedded_node->next = &bbuddy->list[0];
    embedded_node->prev = &bbuddy->list[0];

    for (size_t i = 1 ; i < free_list_size; i++) {
        bbuddy->list[i].prev = &bbuddy->list[i];
        bbuddy->list[i].next = &bbuddy->list[i];
    }
    
    bbuddy->bmask = bmask_mem;
    
    for(size_t i = 0; i < bbuddy->bmask_size; i++){
        bbuddy->bmask[i] = 0;
    }

    bbuddy->size = size_of_allocation;
    bbuddy->free_list_size = free_list_size;
    bbuddy->owns_memory = false;
    bbuddy->heads_bmask = 1;
    return bbuddy;
}

BAD_BBUDDY_DEF bad_bbuddy_t* bad_bbuddy_init_allocate(
        void *bbuddy_mem, 
        void *freelist_mem,
        void *bmask_mem,
        size_t max_order,
        size_t min_order)
{
    bad_bbuddy_t * bbuddy = bbuddy_mem;

    bbuddy->min_order = min_order;
    bbuddy->max_order = max_order;

    size_t size_of_allocation = (size_t)1<<max_order;
    size_t page_size =  sysconf(_SC_PAGESIZE);
    if(size_of_allocation < page_size || max_order < min_order ){
        return NULL;
    }
    
    size_t free_list_size = max_order - min_order + 1;
    bbuddy->base = (uintptr_t)mmap(NULL, size_of_allocation, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    if(bbuddy->base == (uintptr_t)MAP_FAILED){
        return NULL;
    }
     
    bbuddy->list = freelist_mem; 
    bbuddy->bmask_size = (((1 << (max_order - min_order))-1) + (sizeof(size_t)*8) - 1) /(sizeof(size_t)*8);
    bbuddy->list[0].next = (bad_bbuddy_free_node_t *)bbuddy->base;
    bbuddy->list[0].prev =  (bad_bbuddy_free_node_t *)bbuddy->base;
    bad_bbuddy_free_node_t * embedded_node = (bad_bbuddy_free_node_t *)bbuddy->base;
    embedded_node->next = &bbuddy->list[0];
    embedded_node->prev = &bbuddy->list[0];

    for (size_t i = 1 ; i < free_list_size; i++) {
        bbuddy->list[i].prev = &bbuddy->list[i];
        bbuddy->list[i].next = &bbuddy->list[i];
    }
    
    bbuddy->bmask = bmask_mem;
    
    for(size_t i = 0; i < bbuddy->bmask_size; i++){
        bbuddy->bmask[i] = 0;
    }
   
    bbuddy->size = size_of_allocation;
    bbuddy->free_list_size = free_list_size;
    bbuddy->owns_memory = true;
    bbuddy->heads_bmask = 1;
    return bbuddy;
}


BAD_BBUDDY_DEF void* bad_bbuddy_alloc(bad_bbuddy_t *bbuddy,size_t size){
    
    size_t order = (sizeof(size_t) * 8) -__builtin_clzl(size) - !(size & (size-1));
    
    if(order > bbuddy->max_order){
        return NULL;
    }

    if(order < bbuddy->min_order){
        order = bbuddy->min_order;
    }

    size_t idx = bbuddy->max_order  - order;
    size_t order_mask = ((size_t)1 << (idx + 1)) - 1;
    size_t masked = bbuddy->heads_bmask & order_mask;
    if(!masked){
        return NULL;
    }   
    size_t picked_idx = (sizeof(size_t) * 8) - 1 - __builtin_clzl(masked) ;
    
    size_t splits = idx - picked_idx;
    uint8_t *block_for_split = (uint8_t*)bbuddy->list[picked_idx].next;
    bbuddy->list[picked_idx].next = bbuddy->list[picked_idx].next->next;
    bbuddy->list[picked_idx].next->prev = &bbuddy->list[picked_idx];
    bbuddy->heads_bmask ^= (size_t)(&bbuddy->list[picked_idx] == bbuddy->list[picked_idx].next) << picked_idx;
    size_t splited_block_size = (size_t)1 << (bbuddy->max_order - picked_idx-1);
    bad_bbuddy_free_node_t * unused_block = NULL;
    size_t bmaskidx, bmask_word, bmask_bit,offset_from_base;
        
    if(picked_idx){
        offset_from_base = (block_for_split)- (uint8_t*)bbuddy->base;
        bmaskidx = (((size_t)1<<(picked_idx-1))-1) +  ((offset_from_base) >> ( bbuddy->max_order - picked_idx+1));
        bmask_word = bmaskidx / (sizeof(size_t) * 8);
        bmask_bit = bmaskidx & ((sizeof(size_t) * 8) - 1);
        bbuddy->bmask[bmask_word] ^= (size_t)1 << bmask_bit; 
    }
 
    
    for(size_t i =0; i < splits;i++){
        unused_block = (bad_bbuddy_free_node_t *)(block_for_split+splited_block_size);
        unused_block->next = bbuddy->list[picked_idx+1].next;
        bbuddy->list[picked_idx+1].next = unused_block;
        unused_block->prev = &bbuddy->list[picked_idx+1];
        unused_block->next->prev = unused_block;

        offset_from_base = (uint8_t*)unused_block - (uint8_t*)bbuddy->base;
        bmaskidx = (((size_t)1<<(picked_idx))-1) + ((offset_from_base) >> ( bbuddy->max_order - picked_idx));
        bmask_word = bmaskidx / (sizeof(size_t) * 8);
        bmask_bit = bmaskidx & ((sizeof(size_t) * 8) - 1);
        bbuddy->bmask[bmask_word] ^= (size_t)1 << bmask_bit;
        picked_idx++;
        bbuddy->heads_bmask |= ((size_t)1 << picked_idx);
        splited_block_size>>=1;
    }

    return block_for_split;

}

BAD_BBUDDY_DEF void bad_bbuddy_free(bad_bbuddy_t *bbuddy,void *block, size_t size){
    
    size_t order = (sizeof(size_t) * 8) -__builtin_clzl(size) - !(size & (size-1));
    if(order < bbuddy->min_order){
        order = bbuddy->min_order;
    }
    uintptr_t ublock = (uintptr_t) block;
    if( order > bbuddy->max_order || ublock < bbuddy->base || ublock >= bbuddy->base + ((size_t)1 << bbuddy->max_order)){
        return;
    }

    if(__builtin_clzl(ublock) < order){
        return;
    }

    size_t curr_order = order;

    void *curr_block = block;
    size_t idx = 0;
    
    while((idx = bbuddy->max_order - curr_order)){
        size_t buddy_bitmask = (size_t)1 << curr_order;
        
        size_t offset_from_base = (uintptr_t)curr_block - bbuddy->base;
        /*                     level                         pair                       */ 
        size_t bmaskidx =  (((size_t)1<<(idx-1))-1) + ((offset_from_base) >> (curr_order + 1));
        size_t bmask_word = bmaskidx / (sizeof(size_t) * 8);
        size_t bmask_bit = bmaskidx & ((sizeof(size_t) * 8) - 1);

        bbuddy->bmask[bmask_word] ^= (size_t)1 << bmask_bit;
        
        if(bbuddy->bmask[bmask_word] & (size_t)1 << bmask_bit){
            break;
        }
        size_t buddy_offset = offset_from_base ^ buddy_bitmask;
        size_t parent_offset = offset_from_base &(~buddy_bitmask);
        void *buddy_addr = (void*)((uint8_t*)bbuddy->base + buddy_offset);
        void *parent_addr = (void*)((uint8_t*)bbuddy->base + parent_offset); 


        bad_bbuddy_free_node_t *buddy = (bad_bbuddy_free_node_t *)buddy_addr;
        buddy->prev->next = buddy->next;
        buddy->next->prev = buddy->prev;
        buddy->prev = 0;
        buddy->next = 0;
        bbuddy->heads_bmask ^= (size_t)(&bbuddy->list[idx] == bbuddy->list[idx].next) << idx;
        curr_order++;
        curr_block = parent_addr;
    }

    bad_bbuddy_free_node_t *final_block = (bad_bbuddy_free_node_t *)curr_block;
    final_block->next = bbuddy->list[idx].next;
    final_block->next->prev = final_block;
    bbuddy->list[idx].next = final_block;
    final_block->prev = &bbuddy->list[idx];
    bbuddy->heads_bmask |= ((size_t)1 << idx);
}

BAD_BBUDDY_DEF void bad_bbuddy_deinit(bad_bbuddy_t *bbuddy){
    if(bbuddy->owns_memory){
        munmap((void *)bbuddy->base,bbuddy->size);
    }
    *bbuddy = (bad_bbuddy_t){0};
}

BAD_BBUDDY_DEF void bad_bbuddy_reset(bad_bbuddy_t *bbuddy){
    bbuddy->list[0].next = (bad_bbuddy_free_node_t *)bbuddy->base;
    bbuddy->list[0].prev =  (bad_bbuddy_free_node_t *)bbuddy->base;
    bad_bbuddy_free_node_t * embedded_node = (bad_bbuddy_free_node_t *)bbuddy->base;
    embedded_node->next = &bbuddy->list[0];
    embedded_node->prev = &bbuddy->list[0];

    for (size_t i = 1 ; i < bbuddy->free_list_size; i++) {
        bbuddy->list[i].prev = &bbuddy->list[i];
        bbuddy->list[i].next = &bbuddy->list[i];
    }
    
    for(size_t i = 0; i < bbuddy->bmask_size; i++){
        bbuddy->bmask[i] = 0;
    }
    bbuddy->heads_bmask = 1;
}

#endif // BAD_BBUDDY_IMPLEMENTATION
#endif
