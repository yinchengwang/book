#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 01
typedef struct {
    int start;
    int end;
} Window;

typedef struct {
    Window *windows;
    int size;
    int capacity;
} WindowArray;

typedef struct {
    int *data;
    int front;
    int rear;
    int capacity;
} Deque;

Deque *createDeque(int capacity) {
    Deque *dq = (Deque *)malloc(sizeof(Deque));
    dq->data = (int *)malloc(capacity * sizeof(int));
    dq->front = -1;
    dq->rear = -1;
    dq->capacity = capacity;
    return dq;
}

void pushBack(Deque *dq, int value) {
    if (dq->front == -1) {
        dq->front = dq->rear = 0;
    } else {
        dq->rear = (dq->rear + 1) % dq->capacity;
    }
    dq->data[dq->rear] = value;
}

void pushFront(Deque *dq, int value) {
    if (dq->front == -1) {
        dq->front = dq->rear = 0;
    } else {
        dq->front = (dq->front - 1 + dq->capacity) % dq->capacity;
    }
    dq->data[dq->front] = value;
}

int popFront(Deque *dq) {
    if (dq->front == -1)
        return -1;
    int value = dq->data[dq->front];
    if (dq->front == dq->rear) {
        dq->front = dq->rear = -1;
    } else {
        dq->front = (dq->front + 1) % dq->capacity;
    }
    return value;
}

int popBack(Deque *dq) {
    if (dq->front == -1)
        return -1;
    int value = dq->data[dq->rear];
    if (dq->front == dq->rear) {
        dq->front = dq->rear = -1;
    } else {
        dq->rear = (dq->rear - 1 + dq->capacity) % dq->capacity;
    }
    return value;
}

int front(Deque *dq) {
    if (dq->front == -1)
        return -1;
    return dq->data[dq->front];
}

int back(Deque *dq) {
    if (dq->front == -1)
        return -1;
    return dq->data[dq->rear];
}

int isEmpty(Deque *dq) { return dq->front == -1; }

void clearDeque(Deque *dq) { dq->front = dq->rear = -1; }

void freeDeque(Deque *dq) {
    free(dq->data);
    free(dq);
}

WindowArray *createWindowArray(int capacity) {
    WindowArray *wa = (WindowArray *)malloc(sizeof(WindowArray));
    wa->windows = (Window *)malloc(capacity * sizeof(Window));
    wa->size = 0;
    wa->capacity = capacity;
    return wa;
}

void addWindow(WindowArray *wa, int start, int end) {
    if (wa->size >= wa->capacity) {
        wa->capacity *= 2;
        wa->windows = (Window *)realloc(wa->windows, wa->capacity * sizeof(Window));
    }
    wa->windows[wa->size].start = start;
    wa->windows[wa->size].end = end;
    wa->size++;
}

void freeWindowArray(WindowArray *wa) {
    free(wa->windows);
    free(wa);
}

int compareWindows(const void *a, const void *b) {
    Window *wa = (Window *)a;
    Window *wb = (Window *)b;
    return wa->start - wb->start;
}

WindowArray *findLongestTemperatureWindows(int *temps, int n) {
    if (n == 0)
        return createWindowArray(10);

    Deque *maxQ = createDeque(n);
    Deque *minQ = createDeque(n);
    int left = 0;
    int maxLen = 0;
    WindowArray *result = createWindowArray(10);

    for (int right = 0; right < n; right++) {
        int t = temps[right];

        // 如果温度不在有效范围内，重置窗口
        if (t < 18 || t > 24) {
            left = right + 1;
            clearDeque(maxQ);
            clearDeque(minQ);
            continue;
        }

        // 维护最大值队列（递减）
        while (!isEmpty(maxQ) && temps[back(maxQ)] <= t) {
            popBack(maxQ);
        }
        pushBack(maxQ, right);

        // 维护最小值队列（递增）
        while (!isEmpty(minQ) && temps[back(minQ)] >= t) {
            popBack(minQ);
        }
        pushBack(minQ, right);

        // 调整左边界直到温差<=4
        while (!isEmpty(maxQ) && !isEmpty(minQ) && temps[front(maxQ)] - temps[front(minQ)] > 4) {
            if (front(maxQ) == left) {
                popFront(maxQ);
            }
            if (front(minQ) == left) {
                popFront(minQ);
            }
            left++;
        }

        int currentLen = right - left + 1;
        if (currentLen > maxLen) {
            maxLen = currentLen;
            result->size = 0;
            addWindow(result, left, right);
        } else if (currentLen == maxLen && maxLen > 0) {
            addWindow(result, left, right);
        }
    }

    // 按起始索引排序
    qsort(result->windows, result->size, sizeof(Window), compareWindows);

    freeDeque(maxQ);
    freeDeque(minQ);
    return result;
}

// 03
#define MAX_LEN 30
#define MAX_CMDS 30000
#define MAX_RESULT_LEN 100000

typedef struct {
    char cmd[MAX_LEN];
    int distance;
} Command;

typedef struct {
    char result[MAX_RESULT_LEN];
    int found;
} CommandResult;

int min(int a, int b, int c) {
    if (a <= b && a <= c) return a;
    if (b <= a && b <= c) return b;
    return c;
}

int levenshtein_distance(const char *s1, const char *s2) {
    int len1 = strlen(s1);
    int len2 = strlen(s2);
    
    int **dp = (int **)malloc((len1 + 1) * sizeof(int *));
    for (int i = 0; i <= len1; i++) {
        dp[i] = (int *)malloc((len2 + 1) * sizeof(int));
    }
    
    for (int i = 0; i <= len1; i++) {
        dp[i][0] = i;
    }
    for (int j = 0; j <= len2; j++) {
        dp[0][j] = j;
    }
    
    for (int i = 1; i <= len1; i++) {
        for (int j = 1; j <= len2; j++) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            dp[i][j] = min(
                dp[i - 1][j] + 1,
                dp[i][j - 1] + 1,
                dp[i - 1][j - 1] + cost
            );
        }
    }
    
    int result = dp[len1][len2];
    
    for (int i = 0; i <= len1; i++) {
        free(dp[i]);
    }
    free(dp);
    
    return result;
}

int compare_commands(const void *a, const void *b) {
    Command *cmd1 = (Command *)a;
    Command *cmd2 = (Command *)b;
    
    if (cmd1->distance != cmd2->distance) {
        return cmd1->distance - cmd2->distance;
    }
    return strcmp(cmd1->cmd, cmd2->cmd);
}

CommandResult process_commands(int D, int N, char commands[][MAX_LEN], const char *user_input) {
    CommandResult result = {0};
    result.found = 0;
    
    // Check for exact match first
    for (int i = 0; i < N; i++) {
        if (strcmp(commands[i], user_input) == 0) {
            strcpy(result.result, user_input);
            result.found = 1;
            return result;
        }
    }
    
    // Calculate distances and find candidates
    Command candidates[MAX_CMDS];
    int candidate_count = 0;
    
    for (int i = 0; i < N; i++) {
        int distance = levenshtein_distance(user_input, commands[i]);
        if (distance <= D) {
            strcpy(candidates[candidate_count].cmd, commands[i]);
            candidates[candidate_count].distance = distance;
            candidate_count++;
        }
    }
    
    if (candidate_count == 0) {
        strcpy(result.result, "None");
        result.found = 0;
    } else {
        // Sort candidates by distance and then alphabetically
        qsort(candidates, candidate_count, sizeof(Command), compare_commands);
        
        // Build result string with all candidates having the smallest distance
        int min_distance = candidates[0].distance;
        int first = 1;
        
        for (int i = 0; i < candidate_count; i++) {
            if (candidates[i].distance == min_distance) {
                if (!first) {
                    strcat(result.result, "\n");
                }
                strcat(result.result, candidates[i].cmd);
                first = 0;
            }
        }
        result.found = 1;
    }
    
    return result;
}
