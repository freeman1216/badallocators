#pragma once
#ifndef BAD_BUDDY
#define BAD_BUDDY

#include <stdint.h>
#include <stddef.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdbool.h>


#ifndef BAD_BUDDY_DEF
#ifdef BAD_BUDDY_STATIC
    #define BAD_BUDDY_DEF static inline
#else
    #define BAD_BUDDY_DEF extern
#endif
#endif

typedef struct bad_buddy_free_node {
    struct bad_buddy_free_node* next;
}bad_buddy_free_node_t;

typedef struct {
    uintptr_t base;
    size_t heads_bmask;
    size_t min_order;
    size_t max_order;
    size_t size;
    size_t free_list_size;
    bad_buddy_free_node_t **list;
    bool owns_memory;
}bad_buddy_t;


#define BAD_BUDDY_ALIGN (_Alignof(bad_buddy_t))
#define BAD_BUDDY_ALIGN_ATTR __attribute__((aligned(BAD_BUDDY_ALIGN)))

#define BAD_BUDDY_FREELIST_SIZE(max_order,min_order)({\
    _Static_assert(max_order > min_order,"Max order should be bigger then min order");\
    _Static_assert((size_t)1 << (min_order) >= \
            sizeof(bad_buddy_free_node_t),"min_order should be at least log2(sizeof(ptr)");\
    (((max_order) - (min_order) + 1)  * (sizeof(bad_buddy_free_node_t*))); \
})

#define BAD_BUDDY_ALLOC_SIZE(max_order,min_order) ({ \
    _Static_assert(max_order > min_order,"Max order should be bigger then min order");\
    _Static_assert((size_t)1 << (min_order) >= \
            sizeof(bad_buddy_free_node_t),"min_order should be at least log2(sizeof(ptr)");\
    (sizeof(bad_buddy_t) + (((max_order) - (min_order) + 1)  * (sizeof(bad_buddy_free_node_t*)))); \
})

#define BAD_BUDDY_GET_FREELIST_PTR(name)({\
    _Static_assert(sizeof(*(name)) == 1,"Macro requires byte ptr");\
    ((name)+sizeof(bad_buddy_t)); \
})

#define BAD_BUDDY_GET_FREELIST_END(name,max_order,min_order)({\
    _Static_assert(sizeof(*(name)) == 1,"Macro requires byte ptr");\
    (BAD_BUDDY_GET_FREELIST_PTR(name) + (BAD_BUDDY_FREELIST_SIZE((max_order),(min_order)) * \
                                          sizeof(bad_buddy_free_node_t *))); \
})


#define BAD_BUDDY_ALLOCATE_STATIC(name,max_order,min_order)\
    uint8_t BAD_BUDDY_ALIGN_ATTR name##_buddy_mem[BAD_BUDDY_ALLOC_SIZE(max_order,min_order)+((size_t)1 << (max_order))];\
    bad_buddy_t *name = bad_buddy_init(name##_buddy_mem,BAD_BUDDY_GET_FREELIST_PTR(name##_buddy_mem),\
            BAD_BUDDY_GET_FREELIST_END(name##_buddy_mem,(max_order),(min_order)),(max_order),(min_order))

BAD_BUDDY_DEF bad_buddy_t* bad_buddy_init(
        void *buddy_mem,
        void *freelist_mem,
        void *buff,
        size_t max_order,
        size_t min_order);
BAD_BUDDY_DEF bad_buddy_t* bad_buddy_init_allocate(
        void *buddy_mem,
        void *freelist,
        size_t max_order,
        size_t min_order);
BAD_BUDDY_DEF void* bad_buddy_alloc(bad_buddy_t *buddy,size_t size);
BAD_BUDDY_DEF void bad_buddy_free(bad_buddy_t *buddy,void *block, size_t size);
BAD_BUDDY_DEF void bad_buddy_deinit(bad_buddy_t *buddy);

#ifdef BAD_BUDDY_IMPLEMENTATION

BAD_BUDDY_DEF bad_buddy_t *bad_buddy_init(
        void  *buddy_mem,
        void *freelist_mem, 
        void *memory,
        size_t max_order,
        size_t min_order)
{
    bad_buddy_t *buddy = buddy_mem;
    buddy->min_order = min_order;
    buddy->max_order = max_order;
    if(max_order < min_order ){
        return NULL;     
    }

    size_t free_list_size = max_order - min_order + 1;
    size_t size_of_allocation = (size_t)1<<max_order;  
    buddy->list = freelist_mem; 
    buddy->base = (uintptr_t)memory;

    *buddy->list = (bad_buddy_free_node_t *)buddy->base;
    bad_buddy_free_node_t * embedded_node = (bad_buddy_free_node_t *)buddy->base;
    embedded_node->next =NULL;

    for (size_t i = 1 ; i < free_list_size; i++) {
        buddy->list[i] = NULL;  
    }

    buddy->size = size_of_allocation;
    buddy->free_list_size = free_list_size;
    buddy->owns_memory = false;
    buddy->heads_bmask = 1;
    return buddy;
}

BAD_BUDDY_DEF bad_buddy_t *bad_buddy_init_allocate(
        void* buddy_mem,
        void* freelist_mem, 
        size_t max_order,
        size_t min_order)
{
    bad_buddy_t *buddy = buddy_mem; 
    buddy->min_order = min_order;
    buddy->max_order = max_order;

    size_t size_of_allocation = (size_t)1<<max_order;
    size_t page_size = sysconf(_SC_PAGESIZE);
    if(size_of_allocation < page_size || max_order < min_order ){
        return NULL;
    }
    
    
    buddy->base = (uintptr_t)mmap(NULL, size_of_allocation, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    if(buddy->base == (uintptr_t)MAP_FAILED){
        return NULL;
    }
    size_t free_list_size = max_order - min_order + 1;
    buddy->list = freelist_mem; 

    *buddy->list = (bad_buddy_free_node_t *)buddy->base;
    bad_buddy_free_node_t *embedded_node = (bad_buddy_free_node_t *)buddy->base;
    embedded_node->next =NULL;

    for (size_t i = 1 ; i < free_list_size; i++) {
        buddy->list[i] = NULL;  
    }

    buddy->size = size_of_allocation;
    buddy->free_list_size = free_list_size;
    buddy->owns_memory = true;
    buddy->heads_bmask = 1;
    return buddy;
}


BAD_BUDDY_DEF void* bad_buddy_alloc(bad_buddy_t *buddy,size_t size){
    
    size_t order = (sizeof(size_t) * 8) -__builtin_clzl(size) - !(size & (size-1));
    if(order < buddy->min_order){
        order = buddy->min_order;
    }
    if(order > buddy->max_order){
        return NULL;
    }

    size_t idx = buddy->max_order  - order;
    size_t order_mask = ((size_t)1 << (idx + 1)) - 1;
    size_t masked = buddy->heads_bmask & order_mask;
    if(!masked){
        return NULL;
    }
    size_t picked_idx = (sizeof(size_t) * 8) - 1 - __builtin_clzl(masked) ;
    
    size_t splits = idx - picked_idx;
    uint8_t *block_for_split = (uint8_t*)buddy->list[picked_idx];
    buddy->list[picked_idx] = buddy->list[picked_idx]->next;
    buddy->heads_bmask ^= (size_t)(!buddy->list[picked_idx]) << picked_idx;
    size_t splited_block_size = (size_t)1 << (buddy->max_order - picked_idx-1);
    bad_buddy_free_node_t* unused_block = NULL;

    for(size_t i = 0; i < splits; i++){
        unused_block = (bad_buddy_free_node_t*)(block_for_split+splited_block_size);
        unused_block->next = buddy->list[picked_idx+1];
        buddy->list[picked_idx+1] = unused_block;
        
        picked_idx++;
        buddy->heads_bmask |= ((size_t)1 << picked_idx);
        splited_block_size>>=1;
    }

    return block_for_split;
}

BAD_BUDDY_DEF void bad_buddy_free(bad_buddy_t *buddy,void *block, size_t size){
    
    size_t order = (sizeof(size_t) * 8) -__builtin_clzl(size) - !(size & (size-1));

    if( order > buddy->max_order){
        return;
    }
    uintptr_t ublock = (uintptr_t) block;
    if( order > buddy->max_order || ublock < buddy->base || ublock >= buddy->base + ((size_t)1 << buddy->max_order)){
        return;
    }

    if(__builtin_clzl(ublock) < order){
        return;
    }
    size_t curr_order = order; 
    bool found_buddy = false;
    void *curr_block = block;
    size_t idx = 0;

    do{
        idx = buddy->max_order  - curr_order;
        size_t buddy_bitmask = (size_t)1 << curr_order;
        found_buddy = false;
        size_t offset_from_base = (uintptr_t)curr_block - buddy->base;
        void *buddy_addr = (void*)(buddy->base + (uintptr_t)(offset_from_base ^ buddy_bitmask));
        void *parent_addr = (void*)(buddy->base + (uintptr_t)(offset_from_base &(~buddy_bitmask)));

        bad_buddy_free_node_t *traverse = buddy->list[idx];
        bad_buddy_free_node_t *prev = NULL;
  
        if(traverse == buddy_addr){
            found_buddy = true;
            buddy->list[idx] = buddy->list[idx]->next;
            curr_block = parent_addr;
            curr_order++;
            buddy->heads_bmask ^= (size_t)(!buddy->list[idx]) << idx;
            continue;
        }

        while (traverse) {
            prev = traverse;
            traverse = traverse->next;
            if(traverse == buddy_addr){
                prev->next = traverse->next;
                curr_block = parent_addr;
                curr_order++;
                found_buddy = true;
                break;
            }
        } 

    }while (found_buddy);

    bad_buddy_free_node_t *final_block = (bad_buddy_free_node_t *)curr_block;
    final_block->next = buddy->list[idx];
    buddy->list[idx] = final_block;
    buddy->heads_bmask |= (size_t)1 << idx;
}

BAD_BUDDY_DEF void bad_buddy_deinit(bad_buddy_t *buddy){
    if(buddy->owns_memory){
        munmap((void *)buddy->base,buddy->size);
    }
    *buddy = (bad_buddy_t){0};
}

BAD_BUDDY_DEF void bad_buddy_reset(bad_buddy_t *buddy){
    *buddy->list = (bad_buddy_free_node_t *)buddy->base;
    bad_buddy_free_node_t *embedded_node = (bad_buddy_free_node_t *)buddy->base;
    embedded_node->next =NULL;

    for (size_t i = 1 ; i < buddy->free_list_size; i++) {
        buddy->list[i] = NULL;  
    }
    buddy->heads_bmask = 1;
}

#endif // BAD_BUDDY_IMPLEMENTATION
#endif
