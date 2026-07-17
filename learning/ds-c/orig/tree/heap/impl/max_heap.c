#include<stdio.h>
#include<stdlib.h>

typedef int element_type;

typedef struct max_heap_struct {
    element_type *ele_arr;
    int size;
    int capacity;
} max_heap_t;

void heap_item_swap(max_heap_t *heap, int idx1, int idx2)
{
    element_type tmp;
    tmp = heap->ele_arr[idx1];
    heap->ele_arr[idx1] = heap->ele_arr[idx2];
    heap->ele_arr[idx2] = tmp;
}

void heap_print(max_heap_t *heap) {
    for (int i = 0; i < heap->size; i++) {
        printf("%d ", heap->ele_arr[i]);
    }
    printf("\n");
}

max_heap_t* create_heap(int max_size)
{
    max_heap_t *heap = (max_heap_t *)malloc(sizeof(max_heap_t));
    heap->ele_arr = (int *)malloc(sizeof(element_type) * (max_size));
    heap->capacity = max_size;
    heap->size = 0;

    return heap;
}

int is_heap_full(max_heap_t *heap)
{
    return heap->size == heap->capacity;
}

int is_empty(max_heap_t *heap)
{
    return heap->size == 0;
}

int heap_insert(max_heap_t *heap, element_type item)
{
    if (is_heap_full(heap)) {
        printf("max heap is full.\n");
        return 0;
    }
    printf("heap insert: %d.\n", item);

    int target_idx = heap->size;
    heap->ele_arr[target_idx] = item;
    while (target_idx > 0) {
        int parent_idx = (target_idx - 1) / 2;

        if (heap->ele_arr[parent_idx] > heap->ele_arr[target_idx]) {
            break;
        } else {
            heap_item_swap(heap, parent_idx, target_idx);
            target_idx = parent_idx;
        }
    }

    heap->size++;

    return 0;
}

void heap_delete_max(max_heap_t *heap)
{
    if (is_empty(heap)) {
        printf("max heap is empty.\n");
    }

    heap->ele_arr[0] = heap->ele_arr[heap->size - 1];
    heap->size--;

    int parent_id = 0;
    int left_child_id =  parent_id * 2 + 1;
    int right_child_id = left_child_id + 1;

    while (left_child_id < heap->size) {
        int max_child = left_child_id;
        if (right_child_id < heap->size) {
            if (heap->ele_arr[right_child_id] > heap->ele_arr[left_child_id]) {
                max_child = right_child_id;
            } 
        }

        if (heap->ele_arr[parent_id] > heap->ele_arr[max_child]) {
            break;
        } else {
            heap_item_swap(heap, parent_id, max_child);
            parent_id = max_child;
            left_child_id = parent_id * 2 + 1;
            right_child_id = left_child_id + 1;
        }
    }
}

void heap_destroy(max_heap_t *heap) {
    free(heap->ele_arr);
    free(heap);
}

int main()
{
    max_heap_t *heap = create_heap(10);

    heap_insert(heap, 10);
    heap_insert(heap, 50);
    heap_insert(heap, 30);
    heap_insert(heap, 20);
    heap_insert(heap, 40);
    heap_print(heap);

    heap_delete_max(heap);
    heap_print(heap);

    heap_destroy(heap);

    return 0;
}
