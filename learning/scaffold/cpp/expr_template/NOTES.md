# 表达式模板工程对照

## Eigen 库

Eigen 是最著名的表达式模板库：

```cpp
// Eigen 使用表达式模板实现零成本抽象
#include <Eigen/Dense>

Eigen::Vector3d a, b, c;
c = a + b;  // 无临时对象，直接计算

// 复杂表达式
c = 2 * a + b.cross(a);
```

### Eigen 表达式模板结构

```cpp
// Eigen 内部结构（简化）
template<typename Derived>
class DenseBase {
    // 所有表达式继承自 EigenBase
};

template<typename XprType, typename StorageKind>
class Matrix;

// 加法表达式
template<typename Lhs, typename Rhs>
class MatrixAdd : public MatrixBase<MatrixAdd<Lhs, Rhs>> {
    auto operator()(Index i, Index j) const {
        return lhs_(i,j) + rhs_(i,j);  // 惰性求值
    }
};
```

## 项目中的应用场景

### 1. 数值计算库

```cpp
// engineering/rag/ 模块的向量运算
// 可以用表达式模板优化

template<typename E>
class VectorExpr {
    operator const E&() const { return static_cast<const E&>(*this); }
};

// 向量距离计算
template<typename E1, typename E2>
class EuclideanDist {
    auto operator()(size_t i) const {
        auto diff = a_[i] - b_[i];
        return diff * diff;
    }
    // 求和时才计算
    double result() const {
        return std::sqrt(sum_(*this));
    }
};
```

### 2. SQL 表达式求值

```cpp
// engineering/src/db/sql/ 表达式求值
// 表达式模板可以用于优化查询执行

template<typename... Exprs>
class Projection {
    // 投影表达式
};

template<typename Expr>
class Predicate {
    // 谓词表达式
    bool eval(const Row& row) const {
        return expr_.eval(row);  // 惰性求值
    }
};
```

### 3. 图像处理管线

```cpp
// 图像处理链式操作
class ImagePipeline {
    // 表达式模板保存操作链
    // 最后需要输出时才执行
};
```

## 表达式模板 vs 传统方式

| 方式 | 代码 | 临时对象 |
|------|------|----------|
| 传统 | `v3 = v1 + v2;` | 1 个 |
| 表达式 | `v3 = v1 + v2;` | 0 个 |
| 复杂链 | `v4 = v1 + v2 * 3;` | 多个 vs 0 个 |

## 表达式模板的权衡

### 优点
- **零成本抽象**：无运行时开销
- **惰性求值**：延迟计算
- **内存效率**：无临时对象
- **链式操作**：优雅的 DSL

### 缺点
- **编译时间**：模板实例化开销
- **代码复杂度**：类型层次深
- **调试困难**：错误信息复杂
- **编译产物**：代码膨胀

## 何时使用表达式模板

| 场景 | 推荐 |
|------|------|
| 数值计算 | ✓ 性能关键 |
| 向量/矩阵运算 | ✓ |
| 链式转换 | ✓ |
| 简单操作 | ✗ 过度设计 |
| 编译时间敏感 | ✗ |
