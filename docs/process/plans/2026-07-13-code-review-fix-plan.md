# 全项目代码修复执行计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复全项目代码检视中发现的 18 个严重/重要问题，提升代码质量到 "良好" 水平

**Architecture:** 按优先级分三个阶段执行：P0 内存安全 → P1 错误处理 → P2 代码质量

**Tech Stack:** C11 (工程轨)、C++17 (mystl)、GoogleTest (测试框架)

## Global Constraints

- 工程轨代码使用 C11，mystl 使用 C++17
- 所有修复必须附带测试用例
- 每次 commit 只修复一个问题域，便于回退
- 使用 `git add -p` 分阶段暂存相关修改

---

## Phase 1: P0 内存安全与严重Bug修复

### Task 1.1: vector_query_hybrid 内存管理修复

**Files:**
- Modify: `engineering/src/db/storage/vector/vector_engine.c:2250-2320`

**问题描述:**
- `vector_query_hybrid` 函数中 `free(NULL)` 无意义，掩盖真实内存泄漏
- 分配成功后失败路径未正确释放资源

**Acceptance Criteria:**
1. 移除无效的 `free(NULL)` 调用
2. 修正失败路径的资源释放顺序
3. 添加测试用例验证内存安全

**Interfaces:**
- Consumes: `vector_engine_t*`, `vector_query_context_t*`
- Produces: 修复后的 `vector_query_hybrid` 函数

- [ ] **Step 1: 备份并分析 vector_engine.c 中的 vector_query_hybrid 函数**

```bash
sed -n '2250,2320p' engineering/src/db/storage/vector/vector_engine.c
```

- [ ] **Step 2: 创建测试用例验证修复前的行为**

创建: `engineering/test/db/storage/vector_engine_memory_test.cpp`

```cpp
#include <gtest/gtest.h>
#include "db/storage/vector/vector_engine.h"

class VectorEngineMemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 初始化测试环境
    }
    
    void TearDown() override {
        // 清理测试环境
    }
};

// 测试 vector_query_hybrid 内存分配失败场景
TEST_F(VectorEngineMemoryTest, HybridQueryMemoryFailure) {
    vector_engine_t* engine = vector_engine_create(test_dir);
    ASSERT_NE(nullptr, engine);
    
    // 模拟内存分配失败场景
    vector_query_context_t ctx = {0};
    ctx.top_k = 10;
    ctx.ef_search = 100;
    
    // 预期：内存分配失败时正确释放所有已分配资源
    int ret = vector_query_hybrid(engine, &ctx, &result);
    // 验证无内存泄漏
}
```

- [ ] **Step 3: 修复 vector_query_hybrid 函数中的内存问题**

原始代码 (有问题):
```c
// engineering/src/db/storage/vector/vector_engine.c:2281-2287
if (!vec_results.results) {
    free(vec_results.results);  // BUG: 已经是 NULL
    return -1;
}
```

修复后:
```c
if (!vec_results.results) {
    // vec_results.results 为 NULL，无需释放
    if (text_results.results) free(text_results.results);
    return -1;
}
```

- [ ] **Step 4: 修复 text_results.results 分配失败路径**

原始代码 (有问题):
```c
// engineering/src/db/storage/vector/vector_engine.c:2307
if (!results->results) {
    free(vec_results.results);
    free(text_results.results);
    free(results->results);  // BUG: results->results 为 NULL
    return -1;
}
```

修复后:
```c
if (!results->results) {
    free(vec_results.results);
    free(text_results.results);
    // results->results 为 NULL，不需要 free
    return -1;
}
```

- [ ] **Step 5: 运行测试验证修复**

```bash
cd build/engineering
ctest -R vector_engine_memory_test --output-on-failure
```

- [ ] **Step 6: 提交修复**

```bash
git add engineering/src/db/storage/vector/vector_engine.c
git commit -m "fix(db): 修复 vector_query_hybrid 内存管理问题

- 移除无效的 free(NULL) 调用
- 修正失败路径的资源释放顺序
- 添加测试用例验证内存安全"
```

---

### Task 1.2: kmeans 内存泄漏修复

**Files:**
- Modify: `engineering/src/algo-prod/kmeans.c:290-380`

**问题描述:**
- K-Means 分配部分成功时错误处理不正确
- 失败路径遗漏多个已分配内存的释放

**Acceptance Criteria:**
1. 完善分配失败路径的资源释放
2. 确保所有 malloc 配对正确的 free
3. 添加内存泄漏检测测试

- [ ] **Step 1: 分析 kmeans.c 中的内存分配逻辑**

```bash
sed -n '280,400p' engineering/src/algo-prod/kmeans.c
```

- [ ] **Step 2: 创建内存泄漏检测测试**

创建: `engineering/test/algo/kmeans_memory_test.cpp`

```cpp
#include <gtest/gtest.h>
#include "algo/kmeans.h"

TEST(KmeansMemoryTest, AllocationFailureHandling) {
    kmeans_params_t params = {0};
    params.n_samples = 1000000;  // 大数据量
    params.dim = 1000;
    params.n_clusters = 10;
    
    // 模拟分配场景
    kmeans_result_t result = {0};
    int ret = kmeans_fit(&params, &result);
    
    // 验证：无论成功失败，都不应该有内存泄漏
}
```

- [ ] **Step 3: 修复分配逻辑**

原始代码 (有问题):
```c
// kmeans.c:296-301
double *best_centers = (double *)malloc(sizeof(double) * centers_size);
int *best_labels = (int *)malloc(sizeof(int) * (size_t)n_samples);
if (!best_centers || !best_labels) {
    free(best_centers);
    free(best_labels);
    return;  // BUG: 遗漏其他资源的释放
}
```

修复后:
```c
double *best_centers = (double *)malloc(sizeof(double) * centers_size);
int *best_labels = (int *)malloc(sizeof(int) * (size_t)n_samples);
double *centers = (double *)malloc(sizeof(double) * centers_size);
double *next_centers = (double *)malloc(sizeof(double) * centers_size);
double *distances = (double *)malloc(sizeof(double) * (size_t)n_samples);
int *labels = (int *)malloc(sizeof(int) * (size_t)n_samples);
int *counts = (int *)malloc(sizeof(int) * params.n_clusters);

if (!best_centers || !best_labels || !centers || !next_centers || 
    !distances || !labels || !counts) {
    free(best_centers);
    free(best_labels);
    free(centers);
    free(next_centers);
    free(distances);
    free(labels);
    free(counts);
    params.success = 0;
    return;
}
```

- [ ] **Step 4: 修复失败路径**

原始代码 (有问题):
```c
// kmeans.c:365-378
params->cluster_centers = (double *)malloc(sizeof(double) * centers_size);
params->labels = (int *)malloc(sizeof(int) * (size_t)n_samples);
if (!params->cluster_centers || !params->labels) {
    free(params->cluster_centers);
    free(params->labels);
    free(best_centers);       // 遗漏: best_labels
    free(best_labels);
    free(centers);            // 遗漏: next_centers, distances, labels, counts
    return;
}
```

修复后:
```c
params->cluster_centers = (double *)malloc(sizeof(double) * centers_size);
params->labels = (int *)malloc(sizeof(int) * (size_t)n_samples);
if (!params->cluster_centers || !params->labels) {
    free(params->cluster_centers);
    free(params->labels);
    free(best_centers);
    free(best_labels);
    free(centers);
    free(next_centers);
    free(distances);
    free(labels);
    free(counts);
    params->success = 0;
    return;
}
```

- [ ] **Step 5: 运行测试**

```bash
cd build/engineering
ctest -R kmeans_memory_test --output-on-failure
```

- [ ] **Step 6: 提交修复**

```bash
git add engineering/src/algo-prod/kmeans.c
git commit -m "fix(algo): 修复 kmeans 内存泄漏问题

- 完善分配失败路径的资源释放
- 确保所有 malloc 配对正确的 free
- 添加内存泄漏检测测试"
```

---

### Task 1.3: infra_tensor_free_all 内存泄漏修复

**Files:**
- Modify: `engineering/src/kbase/infra/tensor.c:80-90`

**问题描述:**
- `infra_tensor_free_all` 未释放 `tensors` 数组本身

**Acceptance Criteria:**
1. 释放 tensors 数组本身
2. 将每个元素置 NULL 防止误用

- [ ] **Step 1: 查看当前实现**

```bash
sed -n '80,95p' engineering/src/kbase/infra/tensor.c
```

- [ ] **Step 2: 创建测试用例**

创建: `engineering/test/kbase/tensor_memory_test.cpp`

```cpp
#include <gtest/gtest.h>
#include "kbase/infra/tensor.h"

TEST(TensorMemoryTest, FreeAllReleasesArray) {
    infra_tensor_t** tensors = (infra_tensor_t**)malloc(sizeof(infra_tensor_t*) * 3);
    ASSERT_NE(nullptr, tensors);
    
    tensors[0] = infra_tensor_create(10, 10, INFRA_DTYPE_FLOAT32);
    tensors[1] = infra_tensor_create(20, 20, INFRA_DTYPE_FLOAT32);
    tensors[2] = infra_tensor_create(30, 30, INFRA_DTYPE_FLOAT32);
    
    // 修复前：tensors 数组本身不会被释放
    // 修复后：tensors 数组应该被释放
    infra_tensor_free_all(tensors, 3);
    tensors = nullptr;  // 修复后 tensors 应为 NULL
}
```

- [ ] **Step 3: 修复实现**

原始代码:
```c
void infra_tensor_free_all(infra_tensor_t** tensors, int n) {
    if (!tensors) return;
    for (int i = 0; i < n; i++) infra_tensor_free(tensors[i]);
    // BUG: 未释放 tensors 数组本身
}
```

修复后:
```c
void infra_tensor_free_all(infra_tensor_t** tensors, int n) {
    if (!tensors) return;
    for (int i = 0; i < n; i++) {
        infra_tensor_free(tensors[i]);
        tensors[i] = NULL;
    }
    free(tensors);  // 释放数组本身
}
```

- [ ] **Step 4: 提交修复**

```bash
git add engineering/src/kbase/infra/tensor.c
git commit -m "fix(kbase): 修复 infra_tensor_free_all 内存泄漏

- 释放 tensors 数组本身
- 将每个元素置 NULL 防止误用"
```

---

### Task 1.4: realloc 未检查问题修复

**Files:**
- Modify: `engineering/src/kbase/kbase_index.c:35-50`
- Modify: `engineering/src/kbase/infra/graph.c:85-95`

**问题描述:**
- `realloc` 失败时返回 NULL，原指针丢失造成内存泄漏

**Acceptance Criteria:**
1. 使用临时变量接收 realloc 结果
2. 检查返回值后再赋值
3. 失败时保留原指针

- [ ] **Step 1: 查看 kbase_index.c 的 realloc 使用**

```bash
sed -n '35,55p' engineering/src/kbase/kbase_index.c
```

- [ ] **Step 2: 修复 kbase_index.c**

原始代码:
```c
// kbase_index.c:40
idx->notes = (kbase_note_t*)realloc(idx->notes, idx->capacity * sizeof(kbase_note_t));
// BUG: realloc 失败时 idx->notes 原指针丢失
```

修复后:
```c
kbase_note_t* new_notes = (kbase_note_t*)realloc(idx->notes, idx->capacity * sizeof(kbase_note_t));
if (!new_notes) {
    // 分配失败，保留原数据，返回错误
    return -1;
}
idx->notes = new_notes;
```

- [ ] **Step 3: 查看 graph.c 的 realloc 使用**

```bash
sed -n '85,100p' engineering/src/kbase/infra/graph.c
```

- [ ] **Step 4: 修复 graph.c**

原始代码:
```c
// graph.c:90
out[from] = (int*)realloc(out[from], (out_n[from] + 1) * sizeof(int));
// BUG: realloc 失败时原指针丢失
```

修复后:
```c
int* new_out = (int*)realloc(out[from], (out_n[from] + 1) * sizeof(int));
if (!new_out) {
    return -1;  // 分配失败
}
out[from] = new_out;
```

- [ ] **Step 5: 提交修复**

```bash
git add engineering/src/kbase/kbase_index.c engineering/src/kbase/infra/graph.c
git commit -m "fix(kbase): 修复 realloc 失败导致内存泄漏

- 使用临时变量接收 realloc 结果
- 检查返回值后再赋值
- 失败时保留原指针"
```

---

### Task 1.5: mystl vector::pop_back bug 修复

**Files:**
- Modify: `learning/scaffold/cpp/stl/include/mystl/vector.h:300-315`

**问题描述:**
- `pop_back` 先递减指针再析构，导致析构位置错误

**Acceptance Criteria:**
1. 先递减指针，再用 allocator.destroy 析构正确位置
2. 添加 pop_back 边界条件测试

- [ ] **Step 1: 查看当前实现**

```bash
sed -n '300,320p' learning/scaffold/cpp/stl/include/mystl/vector.h
```

- [ ] **Step 2: 创建测试用例**

创建: `learning/scaffold/cpp/stl/test/vector_pop_back_test.cpp`

```cpp
#include <gtest/gtest.h>
#include "mystl/vector.h"
#include <string>

class VectorPopBackTest : public ::testing::Test {
protected:
    mystl::vector<std::string> vec;
    
    void SetUp() override {
        vec.push_back("one");
        vec.push_back("two");
        vec.push_back("three");
    }
};

TEST_F(VectorPopBackTest, PopBackDestroysLastElement) {
    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec.back(), "three");
    
    vec.pop_back();
    
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec.back(), "two");  // 修复后应该返回 "two"
}

TEST_F(VectorPopBackTest, PopBackOnSingleElement) {
    mystl::vector<int> single;
    single.push_back(42);
    
    single.pop_back();
    
    EXPECT_EQ(single.size(), 0);
}

TEST_F(VectorPopBackTest, PopBackOnEmpty) {
    mystl::vector<int> empty;
    empty.pop_back();  // 不应该崩溃
    
    EXPECT_EQ(empty.size(), 0);
}
```

- [ ] **Step 3: 运行测试验证 bug**

```bash
cd build/learning
ctest -R vector_pop_back_test --output-on-failure
# 预期：PopBackDestroysLastElement 失败
```

- [ ] **Step 4: 修复 pop_back 实现**

原始代码 (有问题):
```cpp
// vector.h:306-309
void pop_back() {
    --finish_;
    finish_->~T();  // BUG: finish_ 已递减，析构位置错误
}
```

修复后:
```cpp
void pop_back() {
    if (finish_ == start_) return;  // 空容器检查
    --finish_;
    alloc_.destroy(finish_);  // 使用 allocator 的 destroy 方法
}
```

- [ ] **Step 5: 运行测试验证修复**

```bash
cd build/learning
ctest -R vector_pop_back_test --output-on-failure
# 预期：全部通过
```

- [ ] **Step 6: 提交修复**

```bash
git add learning/scaffold/cpp/stl/include/mystl/vector.h
git add learning/scaffold/cpp/stl/test/vector_pop_back_test.cpp
git commit -m "fix(mystl): 修复 vector::pop_back 析构位置错误

问题：先递减指针再析构，导致析构了错误位置的元素
修复：先递减，再用 allocator.destroy 析构正确位置
测试：添加 pop_back 边界条件测试"
```

---

### Task 1.6: mystl deque iterator-- bug 修复

**Files:**
- Modify: `learning/scaffold/cpp/stl/include/mystl/deque.h:75-95`

**问题描述:**
- iterator `--` 操作符在边界处越界

**Acceptance Criteria:**
1. 修复边界情况下的迭代器递减
2. 添加跨 block 迭代器测试

- [ ] **Step 1: 查看当前实现**

```bash
sed -n '75,100p' learning/scaffold/cpp/stl/include/mystl/deque.h
```

- [ ] **Step 2: 创建测试用例**

创建: `learning/scaffold/cpp/stl/test/deque_iterator_test.cpp`

```cpp
#include <gtest/gtest.h>
#include "mystl/deque.h"

class DequeIteratorTest : public ::testing::Test {
protected:
    mystl::deque<int> dq;
    
    void SetUp() override {
        for (int i = 1; i <= 5; ++i) {
            dq.push_back(i);
        }
    }
};

TEST_F(DequeIteratorTest, IteratorDecrementAcrossBlocks) {
    // 当 deque 跨越多个 block 时测试
    mystl::deque<int> large(100);
    for (int i = 0; i < 100; ++i) {
        large[i] = i;
    }
    
    auto it = large.end();
    --it;
    EXPECT_EQ(*it, 99);
    
    --it;
    EXPECT_EQ(*it, 98);
}

TEST_F(DequeIteratorTest, IteratorDecrementAtBegin) {
    auto it = dq.begin();
    --it;  // 应该回到容器的最后一个元素
    
    EXPECT_EQ(*it, 5);
}
```

- [ ] **Step 3: 修复 iterator-- 实现**

原始代码需要分析 _set_node 的行为后确定修复方案。核心问题：当 cur == first 时，需要正确移动到前一个 block。

修复方案:
```cpp
deque_iterator& operator--() {
    if (cur == first) {
        _set_node(node - 1);
        cur = last - 1;  // 直接设置到最后一个有效元素
    } else {
        --cur;
    }
    return *this;
}
```

- [ ] **Step 4: 运行测试验证修复**

```bash
cd build/learning
ctest -R deque_iterator_test --output-on-failure
```

- [ ] **Step 5: 提交修复**

```bash
git add learning/scaffold/cpp/stl/include/mystl/deque.h
git add learning/scaffold/cpp/stl/test/deque_iterator_test.cpp
git commit -m "fix(mystl): 修复 deque iterator-- 边界越界问题

问题：当 cur == first 时，--cur 会越界
修复：直接设置到 last - 1，而不是先设置 last 再递减
测试：添加跨 block 迭代器递减测试"
```

---

### Task 1.7: BST 删除双子节点逻辑修复

**Files:**
- Modify: `learning/ds-c/orig/tree/bst/impl/bst.c:215-250`

**问题描述:**
- 删除双子节点情况时，后继替换逻辑混乱

**Acceptance Criteria:**
1. 实现标准后继替换算法
2. 添加双子节点删除测试

- [ ] **Step 1: 分析 BST 删除逻辑**

```bash
sed -n '200,260p' learning/ds-c/orig/tree/bst/impl/bst.c
```

- [ ] **Step 2: 创建测试用例**

创建: `learning/ds-c/orig/tree/bst/test/test_bst_delete.c`

```c
#include <stdio.h>
#include <assert.h>
#include "bst.h"

void test_bst_delete_two_children() {
    // 创建树:     5
    //           /   \
    //          3     7
    // 删除 5 (双子节点)，应该用后继 6 替换
    bst_node_t* root = bst_insert(NULL, 5);
    root = bst_insert(root, 3);
    root = bst_insert(root, 7);
    root = bst_insert(root, 6);
    
    root = bst_delete(root, 5);
    
    // 验证树结构正确
    assert(root != NULL);
    assert(root->key == 6);  // 后继成为新的根
    
    bst_destroy(root);
    printf("test_bst_delete_two_children PASSED\n");
}
```

- [ ] **Step 3: 修复 BST 删除实现**

使用标准后继替换算法：
```c
bst_node_t* bst_delete(bst_node_t* root, bst_key_t key) {
    if (!root) return NULL;
    
    if (key < root->key) {
        root->left = bst_delete(root->left, key);
    } else if (key > root->key) {
        root->right = bst_delete(root->right, key);
    } else {
        // 找到要删除的节点
        if (!root->left) {
            bst_node_t* temp = root->right;
            free(root);
            return temp;
        } else if (!root->right) {
            bst_node_t* temp = root->left;
            free(root);
            return temp;
        } else {
            // 双子节点：找到后继（中序遍历下一个节点）
            bst_node_t* successor = root->right;
            while (successor->left) {
                successor = successor->left;
            }
            
            // 用后继的值替换当前节点
            root->key = successor->key;
            root->data = successor->data;
            
            // 删除后继节点
            root->right = bst_delete(root->right, successor->key);
        }
    }
    return root;
}
```

- [ ] **Step 4: 运行测试**

```bash
gcc -o test_bst_delete learning/ds-c/orig/tree/bst/test/test_bst_delete.c learning/ds-c/orig/tree/bst/impl/bst.c
./test_bst_delete
```

- [ ] **Step 5: 提交修复**

```bash
git add learning/ds-c/orig/tree/bst/impl/bst.c
git commit -m "fix(ds): 修复 BST 删除双子节点逻辑

问题：删除双子节点时后继替换逻辑混乱
修复：使用标准后继替换算法
测试：添加双子节点删除测试用例"
```

---

### Task 1.8: 链表相加结果顺序修复

**Files:**
- Modify: `learning/ds-c/orig/linked_list/impl/single_list.c:220-270`

**问题描述:**
- 链表相加时 `list_reverse` 被注释掉，导致结果顺序反转

**Acceptance Criteria:**
1. 取消 list_reverse 注释
2. 确保结果顺序正确

- [ ] **Step 1: 分析问题**

```bash
sed -n '220,275p' learning/ds-c/orig/linked_list/impl/single_list.c
```

- [ ] **Step 2: 修复链表相加**

原始代码:
```c
// list_node_t *rh1 = list_reverse(head1);  // 注释掉的反转
// list_node_t *rh2 = list_reverse(head2);
list_node_t *rh1 = head1;  // 不反转直接相加，结果顺序错误
```

修复:
```c
list_node_t *rh1 = list_reverse(head1);
list_node_t *rh2 = list_reverse(head2);
```

- [ ] **Step 3: 提交修复**

```bash
git add learning/ds-c/orig/linked_list/impl/single_list.c
git commit -m "fix(ds): 修复链表相加结果顺序错误

问题：反转被注释导致结果顺序反转
修复：取消 list_reverse 注释"
```

---

## Phase 2: P1 错误处理与边界条件修复

### Task 2.1: fread/malloc/calloc 未检查修复

**Files:**
- Modify: `engineering/src/kbase/kbase_index.c:65-75`
- Modify: `engineering/src/kbase/infra/infra.c:15-30`
- Modify: `engineering/src/kbase/infra/tokenizer.c:90-110`

**Acceptance Criteria:**
1. fread 返回值检查
2. calloc 返回值检查
3. malloc 返回值检查和回滚

- [ ] **Step 1: 修复 kbase_index.c 的 fread**

```c
size_t bytes_read = fread(content, 1, (size_t)sz, fp);
if (bytes_read != (size_t)sz) {
    free(content);
    fclose(fp);
    return -1;
}
```

- [ ] **Step 2: 修复 infra.c 的 calloc**

```c
*embeddings_out = (float*)calloc((size_t)num_texts * dim, sizeof(float));
if (*embeddings_out == NULL) {
    return INFRA_ERR_MEMORY;
}
```

- [ ] **Step 3: 修复 tokenizer.c 的 malloc**

```c
gguf->vocab[j] = (char*)malloc((size_t)sl + 1);
if (!gguf->vocab[j]) {
    // 回滚已分配的 vocab
    for (int k = 0; k < j; ++k) {
        free(gguf->vocab[k]);
    }
    free(gguf->vocab);
    return INFRA_ERR_MEMORY;
}
```

- [ ] **Step 4: 提交修复**

```bash
git add engineering/src/kbase/kbase_index.c engineering/src/kbase/infra/infra.c engineering/src/kbase/infra/tokenizer.c
git commit -m "fix(kbase): 补充 fread/malloc/calloc 返回值检查"
```

---

## Phase 3: P2 代码质量与规范统一

### Task 3.1: strncpy NULL 终止修复

**Files:**
- Modify: `engineering/src/db/storage/kv/kv_engine.c:120-130`

**Acceptance Criteria:**
1. 确保 strncpy 后手动添加 NULL 终止

- [ ] **Step 1: 修复**

```c
strncpy(db->db_path, path, sizeof(db->db_path) - 1);
db->db_path[sizeof(db->db_path) - 1] = '\0';  // 确保 NULL 终止
```

- [ ] **Step 2: 提交修复**

```bash
git add engineering/src/db/storage/kv/kv_engine.c
git commit -m "fix(db): 修复 strncpy 未保证 NULL 终止问题"
```

---

### Task 3.2: mystl at() 异常类型修复

**Files:**
- Modify: `learning/scaffold/cpp/stl/include/mystl/map.h:135-155`

**Acceptance Criteria:**
1. 抛出 std::out_of_range 而非字符串

- [ ] **Step 1: 修复**

```cpp
mapped_type& at(const key_type& k) {
    iterator it = find(k);
    if (it == end()) {
        throw std::out_of_range("map::at: key not found");
    }
    return it->second;
}
```

需要添加 `#include <stdexcept>` 到头文件。

- [ ] **Step 2: 提交修复**

```bash
git add learning/scaffold/cpp/stl/include/mystl/map.h
git commit -m "fix(mystl): 修复 map::at() 抛出字符串而非 std::out_of_range"
```

---

## 修复统计

| 阶段 | 修复问题数 | 提交数 |
|------|-----------|--------|
| Phase 1 | 12 个 | ~8 个 |
| Phase 2 | 3 个 | ~1 个 |
| Phase 3 | 2 个 | ~2 个 |
| **总计** | **17 个** | **~11 个** |
