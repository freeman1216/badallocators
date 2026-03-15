#include <stdint.h>

#define BAD_SLABS_IMPLEMENTATION
#include "../badslabs.h"

typedef struct {
    size_t t1;
    size_t t2;
} test_t;

int main(){
    uint8_t BAD_SLABS_ALIGN_ATTR mem[BAD_BMASK_SLAB_ALLOC_SIZE(sizeof(test_t),16)];
    bad_bitmask_slab_t *slab =bad_bitmask_slab_init(mem,BAD_BMASK_SLAB_GET_BLOCKS_PTR(mem), sizeof(test_t), 16);
    test_t *t1 = bad_bitmask_slab_alloc(slab);
    test_t *t2 = bad_bitmask_slab_alloc(slab);
    bad_bitmask_slab_free(slab, t1);
    bad_bitmask_slab_free(slab, t2);
    
    uint8_t BAD_SLABS_ALIGN_ATTR mem1[BAD_FREELIST_SLAB_ALLOC_SIZE(sizeof(test_t),16)];
    bad_freelist_slab_t *fslab = bad_freelist_slab_init(mem1,BAD_FREELIST_SLAB_GET_BLOCKS_PTR(mem1), sizeof(test_t), 16);
    t1 = bad_freelist_slab_alloc(fslab);
    t2 = bad_freelist_slab_alloc(fslab);
    // bad_freelist_slab_free(fslab, t1);
    // bad_freelist_slab_free(fslab, t2);
    bad_freelist_slab_reset(fslab);
    return 0;
}
