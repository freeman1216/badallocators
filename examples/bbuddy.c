#define BAD_BBUDDY_IMPLEMENTATION
#include "../badbbuddy.h"
#include "stdio.h"
void print_free_lists(bad_bbuddy_t* cb) {
    printf("Free lists:\n");
    for (size_t i = 0; i < cb->free_list_size; i++) {
        printf("Order %zu: ", cb->max_order - i);
        bad_bbuddy_free_node_t* cur = cb->list[i].next;
        bad_bbuddy_free_node_t* sent = &cb->list[i];
        while (cur != sent) {
            printf("[%p] -> ", cur);
            cur = cur->next;
        }
        printf("NULL\n");
    }
    printf("\n");
}
int main(){
    uint8_t BAD_BBUDDY_ALIGN_ATTR mem[BAD_BBUDDY_ALLOC_SIZE(12, 6)];
    bad_bbuddy_t *bbuddy =bad_bbuddy_init_allocate(mem,BAD_BBUDDY_GET_FREELIST_PTR(mem), BAD_BBUDDY_GET_BMASK_PTR(mem, 12, 6), 12, 6);
    BAD_BBUDDY_ALLOCATE_STATIC(sbbuddy, 12, 6);
    void* b1 = bad_bbuddy_alloc(sbbuddy, 64); // 8 bytes
    print_free_lists(sbbuddy);
    void* b2 = bad_bbuddy_alloc(sbbuddy, 100); // 8 bytes
    print_free_lists(sbbuddy);
    void* b3 = bad_bbuddy_alloc(sbbuddy, 300); // 8 bytes
    print_free_lists(sbbuddy);
    //
    bad_bbuddy_free(sbbuddy, b1, 64);
    print_free_lists(sbbuddy);
    bad_bbuddy_free(sbbuddy, b3, 300);
    print_free_lists(sbbuddy);
    bad_bbuddy_free(sbbuddy, b2, 100);
    print_free_lists(sbbuddy);
    bad_bbuddy_reset(sbbuddy);
    bad_bbuddy_deinit(sbbuddy);
    return 0;
}
