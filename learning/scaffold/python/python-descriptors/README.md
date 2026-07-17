# python-descriptors scaffold

Python 描述符演示——__get__/__set__ + property + ORM 字段验证。

## 复现命令

```bash
cd learning/scaffold/python/python-descriptors
python3 main.py
```

## 预期输出

```
[1] 基础描述符:
    Alice: age=25, score=100
    设置负数 age: age must be positive, got -5 ✓

[2] 延迟属性:
    第一次访问: 2
    [Lazy] 计算 processed_data...
    第二次访问（使用缓存）: 2

=== PASS ===
```

## 关键点

- **描述符协议**：`__get__`, `__set__`, `__delete__`
- **数据描述符**：同时实现 `__get__` 和 `__set__`
- **非数据描述符**：只实现 `__get__`
- **__set_name__**：Python 3.6+ 自动获取属性名

详见 NOTES.md。
