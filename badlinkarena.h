#pragma once
#ifndef BAD_LINK_ARENA
#define BAD_LINK_ARENA

#include <stdint.h>
#include <stddef.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdbool.h>

typedef struct bad_link_arena_node{
    struct bad_link_arena_node *next;
    bool self_allocated;
}bad_link_arena_node_t;

typedef struct{
    bad_link_arena_node_t* next;
    bool self_allocated;
    size_t capacity;
    uintptr_t curr;
    bad_link_arena_node_t *free_list;
}bad_link_arena_t;

typedef enum{
    BAD_LINK_ARENA_ALLOC_FAIL,
    BAD_LINK_ARENA_OK,
    BAD_LINK_ARENA_NOT_A_MEMBER,
    BAD_LINK_ARENA_BAD_ARGUMENTS
} bad_link_arena_status_t;

#ifdef BAD_LINK_ARENA_STATIC
#define BAD_LINK_ARENA_DEF static
#else
#define BAD_LINK_ARENA_DEF extern
#endif

#define BAD_LINK_ARENA_ALIGN (_Alignof(bad_link_arena_t))
#define BAD_LINK_ALIGN_ATTR __attribute__((aligned(BAD_LINK_ARENA_ALIGN)))

BAD_LINK_ARENA_DEF bad_link_arena_t* bad_link_arena_init_allocate(size_t prealoc_count);
BAD_LINK_ARENA_DEF bad_link_arena_t* bad_link_arena_init(void *allocated_buffer,size_t size_in_pages);
BAD_LINK_ARENA_DEF void bad_link_arena_deinit(bad_link_arena_t* arena);
BAD_LINK_ARENA_DEF bad_link_arena_status_t bad_link_arena_pop(bad_link_arena_t* arena, void* point); 
BAD_LINK_ARENA_DEF void bad_link_arena_reset(bad_link_arena_t* arena);
BAD_LINK_ARENA_DEF void* bad_link_arena_alloc(bad_link_arena_t* arena,size_t size, size_t alignment);

#ifdef BAD_LINK_ARENA_IMPLEMENTATION

bad_link_arena_t* bad_link_arena_init_allocate(size_t prealoc_count){
    size_t page_size = sysconf(_SC_PAGESIZE);
    bad_link_arena_t* allocated = mmap(NULL, page_size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    
    if(allocated == MAP_FAILED){
        return NULL;
    }
    allocated->self_allocated = true; 
    if(prealoc_count){
        uintptr_t prealoc = (uintptr_t)mmap(NULL, page_size * prealoc_count, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
        if((void *)prealoc == MAP_FAILED){
            munmap(allocated, page_size);
            return NULL;
        }
        bad_link_arena_node_t *next =(bad_link_arena_node_t *)(prealoc);
        bad_link_arena_node_t *curr = (bad_link_arena_node_t *)(prealoc);
        for(size_t i = 0; i < prealoc_count - 1; ++i){
            curr =(bad_link_arena_node_t *)(prealoc + (i * page_size));
            next = (bad_link_arena_node_t *)(prealoc + ( (i + 1) * page_size));
            curr->next = next;
            curr->self_allocated = true;
        }
        next->next = 0;
        next->self_allocated = true;
        allocated->free_list = (bad_link_arena_node_t *)prealoc;
    }else{
        allocated->free_list = 0;
    }

    allocated->capacity = page_size;
    allocated->curr = (uintptr_t)(allocated+1);
    allocated->next = 0; 
    return allocated; 
}

bad_link_arena_t* bad_link_arena_init(void *allocated_buffer,size_t size_in_pages){
    if(!size_in_pages || !allocated_buffer){
        return NULL;
    }

    size_t page_size = sysconf(_SC_PAGESIZE);
    bad_link_arena_t* allocated =(bad_link_arena_t *) allocated_buffer ;
    allocated->self_allocated = false; 
    if(size_in_pages > 1){
        uintptr_t prealoc = (uintptr_t) allocated_buffer + page_size;

        bad_link_arena_node_t *next = (bad_link_arena_node_t *)(prealoc); 
        bad_link_arena_node_t *curr = (bad_link_arena_node_t *)(prealoc);
        for(size_t i = 0; i < size_in_pages - 2; ++i){
            curr =(bad_link_arena_node_t *)(prealoc + (i * page_size));
            next = (bad_link_arena_node_t *)(prealoc + ( (i + 1) * page_size));
            curr->next = next;
            curr->self_allocated = false;
        }
        next->next = 0;
        next->self_allocated = false;
        allocated->free_list = (bad_link_arena_node_t *)prealoc;
    }else{
        allocated->free_list = 0;
    }

    allocated->capacity = page_size;
    allocated->curr = (uintptr_t)(allocated+1);
    allocated->next = 0;
    return allocated; 
}


void bad_link_arena_deinit(bad_link_arena_t *arena){

    bad_link_arena_node_t *traverse = arena->next;
    bad_link_arena_node_t *prev; 
    while(traverse){
        prev = traverse;
        traverse = traverse->next;
        if(prev->self_allocated){
            munmap(prev, arena->capacity);
        }
    }
    
    traverse = arena->free_list;
    
    while(traverse){
        prev = traverse;
        traverse = traverse->next;
        if(prev->self_allocated){
            munmap(prev, arena->capacity);
        }
    }
    
    if(arena->self_allocated){
        munmap(arena, arena->capacity);
    }
}

bad_link_arena_status_t bad_link_arena_pop(bad_link_arena_t *arena, void *point){
    bad_link_arena_node_t *point_page = (bad_link_arena_node_t *)((uintptr_t)point & ~(arena->capacity-1));
    bad_link_arena_node_t *curr_page = (bad_link_arena_node_t *)(arena->curr & ~(arena->capacity-1));
    uintptr_t upoint = (uintptr_t)point;
    if(point_page == curr_page){
        if(curr_page == (bad_link_arena_node_t *) arena){
            if(upoint < (uintptr_t)(arena+1)){
                return BAD_LINK_ARENA_BAD_ARGUMENTS;
            }
        }else{
            if(upoint < (uintptr_t)(curr_page + 1)){
                return BAD_LINK_ARENA_BAD_ARGUMENTS;
            }
        }
        arena->curr = upoint;
        return BAD_LINK_ARENA_OK;
    }

    if(point_page == (bad_link_arena_node_t *) arena){
        if(upoint < (uintptr_t)(arena+1)){
            return BAD_LINK_ARENA_BAD_ARGUMENTS;
        }
        curr_page->next = arena->free_list;
        arena->free_list = arena->next;
        arena->next = 0;
        arena->curr = (uintptr_t)point;
        return BAD_LINK_ARENA_OK;
    }

    bad_link_arena_node_t *traverse = arena->next;
    while(traverse){
        if(traverse == point_page){
            if(upoint < (uintptr_t)(traverse+1)){
                return BAD_LINK_ARENA_BAD_ARGUMENTS;
            }
            arena->curr = (uintptr_t) point;
            curr_page->next = arena->free_list;
            arena->free_list = traverse->next;
            traverse->next = 0;
            return BAD_LINK_ARENA_OK;
        }
        traverse = traverse->next;
    }

    return BAD_LINK_ARENA_NOT_A_MEMBER;
}

void bad_link_arena_reset(bad_link_arena_t* arena){
    arena->curr = (uintptr_t)(arena+1);
    arena->free_list = arena->next;
    arena->next = 0;
}

static inline bad_link_arena_status_t bad_link_arena_new_link(bad_link_arena_t* arena){
    bad_link_arena_node_t* allocated;
    if(arena->free_list){
        allocated = arena->free_list;
        arena->free_list = arena->free_list->next;
    }else{
        allocated = mmap(NULL, arena->capacity, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
        if(allocated == MAP_FAILED){
            return BAD_LINK_ARENA_ALLOC_FAIL;
        }
        allocated->self_allocated = true;
    }

    bad_link_arena_node_t *prev_page = (bad_link_arena_node_t*)(arena->curr & ~(arena->capacity-1));

    allocated->next = 0;
    prev_page->next = allocated;
    arena->curr = (uintptr_t)(allocated+1);
    return BAD_LINK_ARENA_OK;
}

void* bad_link_arena_alloc(bad_link_arena_t* arena,size_t size, size_t alignment){
    
    if(size > arena->capacity-sizeof(bad_link_arena_node_t *)){
        return NULL;
    }

    if((alignment-1) & alignment || alignment == 0 ){
        //add scream at the programmer here
        return NULL;
    }
    
    uintptr_t curr_page = (arena->curr & ~(arena->capacity-1));
    size_t padding = (alignment - (arena->curr & (alignment - 1)))&(alignment - 1);

    if((arena->curr + size + padding -  curr_page) > arena->capacity){
        if(bad_link_arena_new_link(arena) != BAD_LINK_ARENA_OK){
            return NULL;
        }
        padding = (alignment - (arena->curr & (alignment - 1)))&(alignment - 1); //recalculate since the curr has changed
    }
    
    void* result  = (void *)(arena->curr+padding);
    
    arena->curr += padding + size;
    return result;
}

#endif

#endif
