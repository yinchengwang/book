# Task 8 报告：COPY BINARY 格式与 EXPLAIN YAML 输出

## STATUS: DONE

## COMMITS

```
ff49e446 feat(copy): 添加二进制格式 COPY 支持
```

## 实现内容

### Part 1: COPY BINARY 格式

**文件**: `engineering/src/db/tools/copy/copy_binary.c`

核心特性：
- 使用网络字节序（大端序）保证跨平台兼容性
- 二进制头文件包含：魔数（PGCO）、版本、标志位、字段数
- 字段格式：4字节长度 + 数据，NULL 字段使用 0xFFFFFFFF 标记
- Windows 平台使用内联字节序转换函数，避免 Win32 API 冲突

API：
```c
int binary_write_header(FILE *fp, int num_columns);
int binary_write_row(FILE *fp, const char **values, int num_columns);
int binary_read_header(FILE *fp, int *num_columns);
int binary_read_field(FILE *fp, char **value, uint32_t *len);
```

### Part 2: EXPLAIN YAML 输出

**文件**: `engineering/src/db/tools/explain/explain_yaml.c`

核心特性：
- 符合 YAML 1.2 规范的格式输出
- 支持特殊字符自动转义（使用单引号包围）
- 递归输出嵌套计划节点
- 包含成本信息、行数、宽度等统计

输出示例：
```yaml
- Node Type: Seq Scan
  Relation Name: users
  Filter: '(price > 100)'
  Cost:
    Startup: 0.00
    Total: 8.00
  Plan Rows: 100
  Plan Width: 64
```

### Part 3: 头文件更新

**文件**: `engineering/include/db/tools/copy.h`
- 新增二进制格式 API 声明

**文件**: `engineering/include/db/tools/explain.h`
- 新增 `explain_output_yaml()` 函数声明

### Part 4: 测试

**文件**: `engineering/test/db/tools/test_copy_binary.cpp`
- 7 个测试用例：WriteHeader, WriteRow, WriteRowWithNull, ReadHeader, ReadField, ReadNullField, InvalidMagic

**文件**: `engineering/test/db/tools/test_explain_yaml.cpp`
- 5 个测试用例：OutputYamlSeqScan, OutputYamlIndexScan, OutputYamlNestedPlan, OutputYamlWithFilter, OutputYamlNullNode

### Part 5: CMake 配置

更新文件：
- `engineering/src/db/tools/copy/CMakeLists.txt` - 添加 copy_binary.c
- `engineering/src/db/tools/explain/CMakeLists.txt` - 添加 explain_yaml.c
- `engineering/test/db/tools/CMakeLists.txt` - 添加新测试目标

## TESTS

```
[==========] Running 7 tests from 1 test suite.
[  PASSED  ] 7 tests.

[==========] Running 5 tests from 1 test suite.
[  PASSED  ] 5 tests.
```

所有测试通过。