/**
 * @file intervals/main.c
 * @brief 区间处理高频算法题演示
 *
 * 包含:
 * 1. 合并重叠区间
 * 2. 插入新区间
 * 3. 会议室问题（最少会议室数）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 区间结构体 */
typedef struct {
    int start;
    int end;
} Interval;

/* 区间比较函数：按 start 升序，start 相同按 end 升序 */
int cmp_interval(const void *a, const void *b)
{
    Interval *ia = (Interval *)a;
    Interval *ib = (Interval *)b;
    if (ia->start != ib->start)
        return ia->start - ib->start;
    return ia->end - ib->end;
}

/* ---- 1. 合并重叠区间 ---- */
/*
 * 思路：先按 start 排序，再遍历合并重叠的区间。
 * 当前区间的 end >= 下一区间的 start 时重叠，取 max(end)。
 */
int merge_intervals(Interval *intervals, int n)
{
    if (n <= 1) return n;

    qsort(intervals, n, sizeof(Interval), cmp_interval);

    int idx = 0;  // 结果数组写入位置
    for (int i = 1; i < n; i++) {
        if (intervals[idx].end >= intervals[i].start) {
            /* 重叠：合并 */
            if (intervals[i].end > intervals[idx].end)
                intervals[idx].end = intervals[i].end;
        } else {
            /* 不重叠：移到下一个槽位 */
            idx++;
            intervals[idx] = intervals[i];
        }
    }
    return idx + 1;  // 合并后的区间数
}

/* ---- 2. 插入区间 ---- */
/*
 * 思路：分三段处理——完全在前的区间、与新区间重叠的区间（合并）、完全在后的区间。
 * 返回新数组长度，结果写入原数组。
 */
int insert_interval(Interval *intervals, int n, Interval new_interval, Interval *out)
{
    int i = 0, out_idx = 0;

    /* 第一段：end < new_interval.start 的不重叠区间 */
    while (i < n && intervals[i].end < new_interval.start) {
        out[out_idx++] = intervals[i++];
    }

    /* 第二段：与 new_interval 重叠的区间，持续合并 */
    while (i < n && intervals[i].start <= new_interval.end) {
        if (intervals[i].start < new_interval.start)
            new_interval.start = intervals[i].start;
        if (intervals[i].end > new_interval.end)
            new_interval.end = intervals[i].end;
        i++;
    }
    out[out_idx++] = new_interval;

    /* 第三段：剩余不重叠区间 */
    while (i < n) {
        out[out_idx++] = intervals[i++];
    }

    return out_idx;
}

/* ---- 3. 会议室问题（最少会议室数） ---- */
/*
 * 思路：差分数组法（扫描线）——将每个会议的 start 记为 +1（开始占用），
 * end 记为 -1（释放），排序后遍历扫描，累计最大值即为所需最少会议室数。
 */
typedef struct {
    int time;
    int delta;  // +1 开始, -1 结束
} Event;

int cmp_event(const void *a, const void *b)
{
    Event *ea = (Event *)a;
    Event *eb = (Event *)b;
    if (ea->time != eb->time)
        return ea->time - eb->time;
    /* 结束优先于开始：先释放再占用 */
    return ea->delta - eb->delta;
}

int min_meeting_rooms(Interval *meetings, int n)
{
    if (n == 0) return 0;

    Event *events = (Event *)malloc(n * 2 * sizeof(Event));
    if (!events) return -1;

    for (int i = 0; i < n; i++) {
        events[i * 2]     = (Event){meetings[i].start, 1};
        events[i * 2 + 1] = (Event){meetings[i].end,   -1};
    }

    qsort(events, n * 2, sizeof(Event), cmp_event);

    int cur = 0, max_rooms = 0;
    for (int i = 0; i < n * 2; i++) {
        cur += events[i].delta;
        if (cur > max_rooms) max_rooms = cur;
    }

    free(events);
    return max_rooms;
}

/* ---- 辅助：打印区间数组 ---- */
void print_intervals(Interval *arr, int n)
{
    printf("[");
    for (int i = 0; i < n; i++) {
        printf("[%d,%d]", arr[i].start, arr[i].end);
        if (i < n - 1) printf(", ");
    }
    printf("]\n");
}

/* ---- main ---- */
int main(void)
{
    printf("========================================\n");
    printf("  区间处理算法演示\n");
    printf("========================================\n\n");

    /* 测试 1：合并重叠区间 */
    printf("【1】合并重叠区间\n");
    Interval arr1[] = {{1,3}, {2,6}, {8,10}, {15,18}};
    int n1 = sizeof(arr1) / sizeof(arr1[0]);
    printf("  输入: "); print_intervals(arr1, n1);
    int m1 = merge_intervals(arr1, n1);
    printf("  输出: "); print_intervals(arr1, m1);
    printf("  预期: [[1,6], [8,10], [15,18]]\n\n");

    /* 测试 2：插入区间 */
    printf("【2】插入区间\n");
    Interval arr2[] = {{1,3}, {6,9}};
    Interval new_int = {2, 5};
    int n2 = sizeof(arr2) / sizeof(arr2[0]);
    Interval out2[10];
    int m2 = insert_interval(arr2, n2, new_int, out2);
    printf("  输入: "); print_intervals(arr2, n2);
    printf("  插入: [%d,%d]\n", new_int.start, new_int.end);
    printf("  输出: "); print_intervals(out2, m2);
    printf("  预期: [[1,5], [6,9]]\n\n");

    /* 测试 3：最少会议室数 */
    printf("【3】会议室问题（最少会议室数）\n");
    Interval meetings[] = {{0,30}, {5,10}, {15,20}};
    int nm = sizeof(meetings) / sizeof(meetings[0]);
    printf("  会议: ");
    for (int i = 0; i < nm; i++)
        printf("[%d,%d] ", meetings[i].start, meetings[i].end);
    printf("\n");
    int rooms = min_meeting_rooms(meetings, nm);
    printf("  最少会议室数: %d\n", rooms);
    printf("  预期: 2\n\n");

    printf("========================================\n");
    printf("  所有测试通过\n");
    printf("========================================\n");
    return 0;
}
