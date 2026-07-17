# C 语言单元测试框架速查表

## 手工断言宏（本卡使用）

```c
/* 最小可用测试框架——20 行宏覆盖所有需求 */
static int test_fails = 0;

#define TEST(name)  static void test_##name(void)
#define RUN_TEST(name) do { ... test_##name(); ... } while(0)
#define ASSERT_EQ(a, b) do { if ((a)!=(b)) { fails++; return; } } while(0)
#define ASSERT_TRUE(cond) ASSERT_EQ(cond, 1)
```

## 主流 C 测试框架对比

| 框架 | 特点 | 依赖 | 适用场景 |
|------|------|------|---------|
| **手工宏** | 零依赖，20 行代码 | 无 | 小型项目、学习目的 |
| **Unity** | 轻量级，单头文件 | 仅 unity.h | 嵌入式 C 项目 |
| **CppUTest** | C/C++ 双支持，Mock 插件 | 编译为库 | 嵌入式 TDD |
| **Check** | 独立进程运行测试 | 编译为库 | Linux 守护进程测试 |
| **Google Test + C** | 通过 extern "C" 包裹 C 代码 | gtest 库 | 本仓库 engineering/test/ |

## 测试方法论

```
AAA 模式：
  Arrange   — 构造测试数据
  Act       — 调用被测函数
  Assert    — 验证结果

TEST(factorial_five) {
    int result = factorial(5);   // Arrange + Act
    ASSERT_EQ(result, 120);      // Assert
}
```

## 边界值测试（BVT）

```
factorial 测试用例设计：
  - n = 0   → 1    (边界: 最小值)
  - n = 1   → 1    (边界: 最小正常值)
  - n = 5   → 120  (正常值)
  - n = -1  → -1   (边界: 错误输入)
  - n = 12  → ?    (边界: int 溢出？)
```
