#pragma once
#ifndef BAD_SLABS
#define BAD_SLABS


#include <stdint.h>
#include <stddef.h>

#ifndef BAD_SLABS_DEF
#ifdef BAD_SLABS_STATIC
    #define BAD_ARENA_STATIC
    #define BAD_SLABS_DEF static
#else
    #define BAD_SLABS_DEF extern
#endif
#endif

//Global config
#define BAD_SLABS_USE_BMASK_SLAB
#define BAD_SLABS_USE_FREELIST_SLAB

#define BAD_SLABS_ALIGN (_Alignof(double))
#define BAD_SLABS_ALIGN_ATTR __attribute__((aligned(BAD_SLABS_ALIGN)))

#ifdef BAD_SLABS_USE_BMASK_SLAB

typedef struct bitmask_slab_cb{
    size_t free_bitmask;
    uint8_t capacity;
    size_t block_size;
    uintptr_t blocks;
} bad_bitmask_slab_t;

#define BAD_BMASK_SLAB_ALLOC_SIZE(sizeof_member_type, capacity) ({ \
    _Static_assert(capacity <= sizeof(size_t) * 8, "Capacity exceeds bits in size_t"); \
    (sizeof(bad_bitmask_slab_t) + ((capacity) * (sizeof_member_type))); \
})

#define BAD_BMASK_SLAB_GET_BLOCKS_PTR(name)({\
    _Static_assert(sizeof(*(name)) == 1,"Macro requires byte ptr");\
    ((name)+sizeof(bad_bitmask_slab_t)); \
})

#define BAD_BMASK_SLAB_ALOCATE_STATIC(name,sizeof_member_type,capacity)\
    _Static_assert(capacity > 0,"Capacity <= 0");\
    uint8_t BAD_SLABS_ALIGN_ATTR name##_bslab_mem[BAD_BMASK_SLAB_ALLOC_SIZE(sizeof_member_type,capacity)];\
    bad_bitmask_slab_t *name = bad_bitmask_slab_init(name##_bslab_mem, BAD_BMASK_SLAB_GET_BLOCKS_PTR(name##_bslab_mem),\
            sizeof_member_type,capacity)



BAD_SLABS_DEF bad_bitmask_slab_t* bad_bitmask_slab_init(
        void *bslab_mem,
        void *blocks, 
        size_t sizeof_member_type,
        uint8_t capacity);
BAD_SLABS_DEF void* bad_bitmask_slab_alloc(bad_bitmask_slab_t *slab);
BAD_SLABS_DEF void bad_bitmask_slab_free(bad_bitmask_slab_t *slab,void *block);
BAD_SLABS_DEF void bad_bitmask_slab_reset(bad_bitmask_slab_t *slab);
#endif // BAD_SLABS_USE_BMASK_SLAB

#ifdef BAD_SLABS_USE_FREELIST_SLAB

typedef struct bad_freelist_node{
    struct bad_freelist_node *next;
} bad_freelist_node_t;

typedef struct freelist_slab_cb{
    bad_freelist_node_t *next_free;
    size_t capacity;
    size_t block_size;
    uintptr_t blocks;   
} bad_freelist_slab_t;

#define BAD_FREELIST_SLAB_ALLOC_SIZE(sizeof_member_type, capacity) ({ \
    _Static_assert(capacity > 1 ,"Capacity cant be 1");\
    _Static_assert(sizeof_member_type >= sizeof(void*), "Type should be at least a size of a ptr"); \
    (sizeof(bad_freelist_slab_t) + ((capacity) * (sizeof_member_type))); \
})

#define BAD_FREELIST_SLAB_GET_BLOCKS_PTR(name)({\
    _Static_assert(sizeof(*(name)) == 1,"Macro requires byte ptr");\
    ((name)+sizeof(bad_freelist_slab_t)); \
})

#define BAD_FREELIST_SLAB_ALOCATE_STATIC(name,sizeof_member_type,capacity)\
_Static_assert(capacity > 1,"Capacity <= 1");\
uint8_t BAD_SLABS_ALIGN_ATTR name##_fslab_mem[BAD_BMASK_SLAB_ALLOC_SIZE(sizeof_member_type,capacity)];\
bad_freelist_slab_t *name = bad_freelist_slab_init(name##_fslab_mem,BAD_FREELIST_SLAB_GET_BLOCKS_PTR(name##_fslab_mem),\
        sizeof_member_type,capacity)

BAD_SLABS_DEF bad_freelist_slab_t* bad_freelist_slab_init(
        void *fslab_mem,
        void *flist_mem, 
        size_t sizeof_member_type, 
        size_t capacity);
BAD_SLABS_DEF void* bad_freelist_slab_alloc(bad_freelist_slab_t *slab);
BAD_SLABS_DEF void bad_freelist_slab_free(bad_freelist_slab_t *slab, void *block);
BAD_SLABS_DEF void bad_freelist_slab_reset(bad_freelist_slab_t *slab);
#endif // BAD_SLABS_USE_FREELIST_SLAB

#if defined (BAD_SLABS_IMPLEMENTATION) && defined (BAD_SLABS_USE_BMASK_SLAB)
BAD_SLABS_DEF bad_bitmask_slab_t* bad_bitmask_slab_init(void *bslab_mem,void *blocks, size_t sizeof_member_type, uint8_t capacity){
    bad_bitmask_slab_t *slab =  bslab_mem;
    slab->blocks = (uintptr_t)blocks;
    if(capacity == sizeof(size_t) * 8){
        slab->free_bitmask = ~((size_t)0);
    }else{
        slab->free_bitmask = ((size_t)1<<capacity)-1;
    }
    slab->block_size = sizeof_member_type;
    slab->capacity = capacity;
    return slab;
}

BAD_SLABS_DEF void* bad_bitmask_slab_alloc(bad_bitmask_slab_t *slab){
    if(slab->free_bitmask == 0){
        return NULL;
    }
    uint8_t block_idx = __builtin_ctzl(slab->free_bitmask);
    slab->free_bitmask &= ~((size_t)1<<block_idx);
    return (void *)(slab->blocks+(block_idx*slab->block_size));
}

BAD_SLABS_DEF void bad_bitmask_slab_free(bad_bitmask_slab_t *slab, void *block){
    uintptr_t handyptr = (uintptr_t) block;

    uintptr_t blocks = slab->blocks;
    if(handyptr < blocks || handyptr >= blocks + (slab->block_size * slab->capacity )){
        return;
    }
    uint8_t block_idx = ((uintptr_t)block - blocks)/slab->block_size;
    slab->free_bitmask |= ((size_t)1<<block_idx); 
}

BAD_SLABS_DEF void bad_bitmask_slab_reset(bad_bitmask_slab_t *slab){
    if(slab->capacity == sizeof(size_t) * 8){
        slab->free_bitmask = ~((size_t)0);
    }else{
        slab->free_bitmask = ((size_t)1<<slab->capacity)-1;
    }
}

#endif // BAD_SLABS_BMASK_SLAB_IMPLEMENTATION

#if defined (BAD_SLABS_IMPLEMENTATION) && defined (BAD_SLABS_USE_FREELIST_SLAB)

BAD_SLABS_DEF bad_freelist_slab_t* bad_freelist_slab_init(void *fslab_mem,void* blocks,size_t size,size_t capacity){
    if(capacity < 2){
        return NULL;
    }

    bad_freelist_slab_t *slab = fslab_mem;  
    slab->next_free = blocks;
    
    bad_freelist_node_t *prev = blocks;
    bad_freelist_node_t *next = blocks;
     
    for(size_t i = 1 ;i<capacity;i++){
        next = (bad_freelist_node_t *)((uintptr_t)prev + size);
        prev->next = next;
        prev = next;
    }
    next->next = NULL;
    
    slab->block_size = size;
    slab->capacity = capacity;
    slab->blocks = (uintptr_t) blocks;
    return slab;
}

BAD_SLABS_DEF void* bad_freelist_slab_alloc(bad_freelist_slab_t *slab){
    if(slab->next_free == 0){
        return NULL;
    }
    void* allocated_ptr = slab->next_free;
    slab->next_free =  slab->next_free->next;
    return allocated_ptr;
}

BAD_SLABS_DEF void bad_freelist_slab_free(bad_freelist_slab_t *slab, void *block ){
    uintptr_t handyptr = (uintptr_t)block;

    uintptr_t blocks = slab->blocks;
    
    if(handyptr < blocks || handyptr >= blocks +(slab->block_size * slab->capacity )){
        return;
    }
    
    bad_freelist_node_t *very_handy_ptr = block;
    very_handy_ptr->next = slab->next_free;
    slab->next_free = very_handy_ptr;
}

BAD_SLABS_DEF void bad_freelist_slab_reset(bad_freelist_slab_t *slab){
    slab->next_free = (bad_freelist_node_t *)(slab->blocks);
    bad_freelist_node_t *prev = (bad_freelist_node_t *)(slab->blocks);
    bad_freelist_node_t *next = (bad_freelist_node_t *)(slab->blocks);
    for(size_t i = 1 ;i<slab->capacity;i++){
        next = (bad_freelist_node_t *)((uintptr_t)prev + slab->block_size);
        prev->next = next;
        prev =prev->next;
    }
    next->next = NULL;

}

#endif // BAD_SLABS_FREELIST_SLAB_IMPLEMENTATION

#endif// BAD_SLABS
