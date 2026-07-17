/* 优先队列
 * 有序数组实现, 二插堆实现, 以及性能对比
*/

#include<stdio.h>
#include<stdlib.h>
#include<stdarg.h>
#include<string.h>
#include<time.h>

#define DEBUG_LOG_SWITCH 0
#define STATUS_OK 0

#if DEBUG_LOG_SWITCH == 1
#define MAX_QUEUE_SIZE 3
#define MAX_QUEUE_PUSH_TIMES 15
#define RANDOM_MIN_NUM 1
#define RANDOM_MAX_NUM 100
#else
#define MAX_QUEUE_SIZE 512
#define MAX_QUEUE_PUSH_TIMES 10000000
#define RANDOM_MIN_NUM 1
#define RANDOM_MAX_NUM 1000000
#endif

typedef struct priority_queue {
    int cap;
    int current;
    int *array;
} priority_queue_t;

void free_ex(void *ptr)
{
    if (ptr != NULL) {
        free(ptr);
        ptr = NULL;
    }
}

void my_printf(const char *format, ...) {
    if (DEBUG_LOG_SWITCH) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}

int generate_random_number(int min, int max, int *random_num)
{
    if (min > max) {
        printf("generate random number range is invaild.\n");
        return -1;
    }

    int range = max - min + 1;
    *random_num = min + rand() % range;
    return STATUS_OK;
}

priority_queue_t *priority_queue_init(size_t queue_size)
{
    if (queue_size <= 0 || queue_size > MAX_QUEUE_SIZE) {
        return NULL;
    }

    priority_queue_t *new_que = (priority_queue_t *)malloc(sizeof(priority_queue_t));
    if (new_que == NULL) {
        printf("malloc new queue failed.\n");
        return NULL;
    }

    new_que->cap = queue_size;
    new_que->current = 0;
    new_que->array = (int *)malloc(sizeof(int) * queue_size);
    if (new_que->array == NULL) {
        printf("malloc new queue failed.\n");
        free(new_que);
        return NULL;
    }

    return new_que;
}

void priority_queue_destroy(priority_queue_t *que)
{
    free_ex(que->array);
    free_ex(que);
}

void order_array_try_push(priority_queue_t *que, int ele)
{
    if (que->current == que->cap) {
        my_printf("order array push %d.\n", ele);
        if (ele > que->array[que->cap - 1]) {
            return;
        }
    }

    int left = 0, right = que->current - 1;
    while (left <= right) {
        // 仅是测试，暂不考虑溢出
        int mid = (left + right) >> 1;
        if (que->array[mid] > ele) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }

    int remain_size = que->current - left;
    memmove(que->array + left + 1, que->array + left, remain_size);
    que->array[left] = ele;
    if (que->current != que->cap) {
        que->current++;
    }
}

void priority_queue_print(priority_queue_t *que)
{
    if (DEBUG_LOG_SWITCH) {
        printf("priority queue array:\n");
        for (int i = 0; i < que->current; i++) {
            printf("%d ", que->array[i]);
        }
        printf("\n");
    }
}

void heap_sift_up(priority_queue_t *que, int start_idx)
{
    if (start_idx > que->current) {
        printf("sift up start idx is greater than current.\n");
        return;
    }

    int target_ele = que->array[start_idx];
    int pos = start_idx;
    while (pos > 0) {
        int parent = (pos - 1) / 2;
        if (target_ele > que->array[parent]) {
            que->array[pos] = que->array[parent];
            pos = parent;
            continue;
        }
        break;
    }

    que->array[pos] = target_ele;
}

void heap_sift_down(priority_queue_t *que, int start_idx)
{
    if (start_idx > que->current) {
        printf("sift down start idx is greater than current.\n");
        return;
    }

    int target_ele = que->array[start_idx];
    int pos = start_idx;
    int child_idx = pos * 2 + 1;
    while (child_idx < que->current) {
        if (child_idx + 1 < que->current) {
            if (que->array[child_idx + 1] > que->array[child_idx]) {
                child_idx = child_idx + 1;
            }
        }

        if (que->array[child_idx] > que->array[pos]) {
            que->array[pos] = que->array[child_idx];
            pos = child_idx;
            child_idx = pos * 2 + 1;
            continue;
        }
        break;
    }

    que->array[pos] = target_ele;
}

void heap_try_push(priority_queue_t *que, int ele)
{
    my_printf("heap push %d.\n", ele);
    if (que->current == que->cap) {
        if (ele > que->array[0]) {
            return;
        }
    }

    if (que->current == que->cap) {
        que->array[0] = que->array[que->current - 1];
        que->current--;
        heap_sift_down(que, 0);
    }

    que->array[que->current] = ele;
    que->current++;
    heap_sift_up(que, que->current - 1);

    return;
}

// int main()
// {
//     srand((unsigned int)time(NULL));

//     priority_queue_t *prio_que = priority_queue_init(MAX_QUEUE_SIZE);
//     if (prio_que == NULL) {
//         printf("prio_que init failed.\n");
//         exit(-1);
//     }

//     int random_numbers[MAX_QUEUE_PUSH_TIMES];
//     for (int i = 0; i < MAX_QUEUE_PUSH_TIMES; i++) {
//         int ret = generate_random_number(RANDOM_MIN_NUM, RANDOM_MAX_NUM, &random_numbers[i]);
//         if (ret != STATUS_OK) {
//             printf("generate random number failed.\n");
//             exit(-1);
//         }
//     }

//     clock_t start, end;

//     start = clock();
//     for (int i = 0; i < MAX_QUEUE_PUSH_TIMES; i++) {
//         order_array_try_push(prio_que, random_numbers[i]);
//         priority_queue_print(prio_que);
//     }
//     end = clock();
//     double array_cpu_time_used = ((double) (end - start)) * 1000 / CLOCKS_PER_SEC;

//     start = clock();
//     for (int i = 0; i < MAX_QUEUE_PUSH_TIMES; i++) {
//         heap_try_push(prio_que, random_numbers[i]);
//         priority_queue_print(prio_que);
//     }
//     end = clock();
//     double heap_cpu_time_used = ((double) (end - start)) * 1000 / CLOCKS_PER_SEC;

//     // printf("Start time: %ld\n", start);
//     // printf("End time: %ld\n", end);
//     printf("Order array push %d items time delay: %f milliseconds\n", MAX_QUEUE_PUSH_TIMES, array_cpu_time_used);
//     printf("heap push %d items time delay: %f milliseconds\n", MAX_QUEUE_PUSH_TIMES, heap_cpu_time_used);

//     priority_queue_destroy(prio_que);

//     return 0;
// }