#include <stdint.h>
#define BAD_ARENA_IMPLEMENTATION
#include "../badarena.h" 

#define TST_ARENA_SIZE 1024

int main(){
    uint8_t BAD_ARENA_ALIGN_ATTR mem[TST_ARENA_SIZE];
    bad_arena_t *arena = bad_arena_init(mem,BAD_ARENA_GET_DATA_PTR(mem), TST_ARENA_SIZE);
    uint8_t *t1 = bad_arena_alloc(arena, 256, 1);
    uint8_t *t2 = bad_arena_alloc(arena, 256, 1);
    bad_arena_pop(arena,t2);
    bad_arena_pop(arena,t1);
    return 0;
}
