#define BAD_BUDDY_IMPLEMENTATION
#include "../badbuddy.h"
#include "stdio.h"
void print_free_lists(bad_buddy_t* cb) {
    printf("Free lists:\n");
    for (size_t i = 0; i < cb->free_list_size; i++) {
        printf("Order %zu: ", cb->max_order - i);
        bad_buddy_free_node_t* cur = cb->list[i];
        while (cur) {
            printf("[%p] -> ", cur);
            cur = cur->next;
        }
        printf("NULL\n");
    }
    printf("\n");
}

int main(){
    uint8_t BAD_BUDDY_ALIGN_ATTR mem[BAD_BUDDY_ALLOC_SIZE(12, 6)];
    bad_buddy_t *buddy = bad_buddy_init_allocate(mem,BAD_BUDDY_GET_FREELIST_PTR(mem) , 12, 6);
    BAD_BUDDY_ALLOCATE_STATIC(sbuddy, 12, 6);
    void* b1 = bad_buddy_alloc(sbuddy, 64); // 8 bytes
    print_free_lists(sbuddy);
    void* b2 = bad_buddy_alloc(sbuddy, 100); // 8 bytes
    print_free_lists(sbuddy);
    void* b3 = bad_buddy_alloc(sbuddy, 300); // 8 bytes
    print_free_lists(sbuddy);
    //
    bad_buddy_free(sbuddy, b1, 64);
    print_free_lists(sbuddy);
    bad_buddy_free(sbuddy, b3, 300);
    print_free_lists(sbuddy);
    bad_buddy_free(sbuddy, b2, 100);
    print_free_lists(sbuddy);
    bad_buddy_reset(sbuddy);
    bad_buddy_deinit(sbuddy);
    return 0;
}
