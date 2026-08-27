# P2-3 空间分析函数 提案

## 背景

多模态能力补齐系列中，P2-3 空间分析函数是支持地理空间数据处理的关键模块。支持点、线、面等几何对象的空间查询和分析操作。

## 变更范围

### 新增文件
| 文件 | 说明 |
|------|------|
| `engineering/include/db/spatial/spatial_index.h` | 空间索引接口 |
| `engineering/src/db/spatial/spatial_index.c` | 空间索引实现（R-Tree） |
| `engineering/src/db/spatial/spatial_ops.c` | 空间操作函数 |
| `engineering/test/db/spatial/spatial_test.cpp` | 空间测试 |

### 修改文件
| 文件 | 说明 |
|------|------|
| `engineering/src/db/spatial/CMakeLists.txt` | 注册空间模块 |

## 核心功能

1. **几何类型**
   - Point（点）
   - LineString（线）
   - Polygon（多边形）
   - GeometryCollection（几何集合）

2. **空间索引**
   - R-Tree 索引构建
   - 范围查询
   - KNN 查询

3. **空间谓词**
   - ST_Contains：包含关系
   - ST_Intersects：相交关系
   - ST_Distance：距离计算
   - ST_Within：被包含关系

4. **空间操作**
   - ST_Area：面积计算
   - ST_Length：长度计算
   - ST_Centroid：重心计算
   - ST_Buffer：缓冲区生成

## 验收标准

- [ ] 空间索引构建测试通过
- [ ] 范围查询测试通过
- [ ] 空间谓词计算正确
- [ ] 几何操作函数正确

## 风险与缓解

| 风险 | 缓解 |
|------|------|
| 复杂几何计算精度 | 使用 double 精度，容差比较 |
| 大数据集索引性能 | 分层 R-Tree 优化 |
