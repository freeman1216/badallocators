#define BAD_LINK_ARENA_IMPLEMENTATION
#include "../badlinkarena.h"



int main(){
    bad_link_arena_t *arena = bad_link_arena_init_allocate(0);
    uint8_t *test1 = bad_link_arena_alloc(arena, 2000, 1);
    uint8_t *test2 = bad_link_arena_alloc(arena, 2000, 1);
    uint8_t *test3 = bad_link_arena_alloc(arena, 2000, 1);
    uint8_t *test4 = bad_link_arena_alloc(arena, 2000, 1);
    uint8_t *test5 = bad_link_arena_alloc(arena, 2000, 1);
    uint8_t *test6 = bad_link_arena_alloc(arena, 2000, 1);
    uint8_t *test7 = bad_link_arena_alloc(arena, 2000, 1);
    uint8_t *test8 = bad_link_arena_alloc(arena, 2000, 1);
    uint8_t *test9 = bad_link_arena_alloc(arena, 2000, 1);
    bad_link_arena_pop(arena,test4);
    bad_link_arena_deinit(arena);
    return 0;
}
