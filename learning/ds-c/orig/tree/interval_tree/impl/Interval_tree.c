/* 区间树(Interval tree)
 * 一种二叉搜索树。它将一个区间划分成一些单元区间 (即单个数据)，每个单元区间对应一个叶节点，非叶节点表示其所代表的子树对应的子区间。
*/

#include<stdio.h>
#include<stdlib.h>

#define TREE_SIZE 10

struct ItvNode{
    int value;
} itvTree[TREE_SIZE * 2];

void build(int node, int begin, int end)
{
    (void)node; (void)begin; (void)end; // Suppress unused parameter warnings
    return;
}

int query(int node, int begin, int end, int left, int right)
{
    (void)node; (void)begin; (void)end; (void)left; (void)right; // Suppress unused parameter warnings
    return 0;
}

void update(int node, int begin, int end, int index, int value)
{
    (void)node; (void)begin; (void)end; (void)index; (void)value; // Suppress unused parameter warnings
    return;
}
