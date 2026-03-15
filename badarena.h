#pragma once
#ifndef BAD_ARENA
#define BAD_ARENA

#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct{
    size_t capacity;
    uint8_t* curr;
    bool owns_memory;
    uintptr_t memory;
}bad_arena_t;


#ifndef BAD_ARENA_DEF
#ifdef BAD_ARENA_STATIC
    #define BAD_ARENA_DEF static inline
#else
    #define BAD_ARENA_DEF extern
#endif
#endif

#define BAD_ARENA_ALIGN (_Alignof(bad_arena_t))
#define BAD_ARENA_ALIGN_ATTR __attribute__((aligned(BAD_ARENA_ALIGN)))

BAD_ARENA_DEF bad_arena_t* bad_arena_init(void *arena_mem,void *data_mem,size_t size_in_bytes);
BAD_ARENA_DEF bad_arena_t* bad_arena_init_allocate(size_t size_in_pages);
BAD_ARENA_DEF void bad_arena_init(bad_arena_t *arena);
BAD_ARENA_DEF void bad_arena_pop(bad_arena_t *arena, void *point);
BAD_ARENA_DEF void bad_arena_reset(bad_arena_t *arena);
BAD_ARENA_DEF void* bad_arena_alloc(bad_arena_t *arena,size_t size, size_t alignment);

#define BAD_ARENA_GET_DATA_PTR(name)({\
    _Static_assert(sizeof(*(name)) == 1,"Macro requires byte ptr");\
    ((name)+sizeof(bad_arena_t)); \
})

#define BAD_ARENA_ALOCATE_STATIC(name,size)\
    _Static_assert(size > 0,"Size < 0");\
    uint8_t BAD_ARENA_ALIGN_ATTR name##_arena_mem[sizeof(bad_arena_t) + size];\
    bad_arena_t *name = bad_arena_init(name##_arena_mem,BAD_ARENA_GET_DATA_PTR(name##_arena_mem),size)
    
#ifdef BAD_ARENA_IMPLEMENTATION
bad_arena_t *bad_arena_init(void *arena_mem,void* data_mem, size_t size_in_bytes){
    if(size_in_bytes <= sizeof(bad_arena_t)){
        return NULL;
    }
    bad_arena_t *arena = arena_mem;
    arena->curr = data_mem;
    arena->memory = (uintptr_t) data_mem;
    arena->capacity = size_in_bytes;
    return arena;
} 

bad_arena_t *bad_arena_init_allocate(size_t size_in_pages){
    size_t page_size = sysconf(_SC_PAGESIZE);
    bad_arena_t *allocated = mmap(NULL, page_size * size_in_pages, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    if(allocated == MAP_FAILED){
        return NULL;
    }
    allocated->capacity = (page_size * size_in_pages) - sizeof(bad_arena_t);
    allocated->curr = (uint8_t*)(allocated + 1);
    allocated->memory = (uintptr_t)(allocated + 1); 
    return allocated; 
}


void bad_arena_deinit(bad_arena_t* arena){
    if(arena->owns_memory){
        munmap(arena, arena->capacity + sizeof(bad_arena_t));
    }
    *arena = (bad_arena_t){0};
}

void bad_arena_pop(bad_arena_t *arena, void *point){
    void *handyptr = ((uint8_t*)(arena->memory)+arena->capacity);
    if(point > handyptr || point < (void*)(arena->memory)){
        //add scream at the programmer here
        return;
    }
    arena->curr = (uint8_t *) point;
}

void bad_arena_reset(bad_arena_t *arena){
    arena->curr = (uint8_t *)arena->memory;
}

void* bad_arena_alloc(bad_arena_t* arena,size_t size, size_t alignment){

    if((alignment-1) & alignment || alignment == 0 ){
        //add scream at the programmer here
        return NULL;
    }

    size_t padding = (alignment - ((uintptr_t)arena->curr & (alignment - 1)))&(alignment - 1);
    size_t diff = arena->curr+size+padding - (uint8_t*) (arena->memory)  ;
    if(diff > arena->capacity){
        //add scream at the programmer here 
        return NULL;
    }
    
    void* result  = arena->curr+padding;
    
    arena->curr += padding+size;
    return result;
}

#endif // BAD_ARENA_IMPLEMENTATION
#endif
