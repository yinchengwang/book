# python-security scaffold

Python 安全编程演示——密码存储 + SQL 注入防御 + XSS。

## 复现命令

```bash
cd learning/scaffold/python/python-security
python3 main.py
```

## 关键点

- **密码哈希**：使用 pbkdf2_hmac + salt
- **SQL 注入**：参数化查询
- **XSS**：HTML 转义
- **输入验证**：正则表达式验证
- **安全随机**：secrets 模块

详见 NOTES.md。
