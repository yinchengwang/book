# 安全设计

## 简介

系统设计中安全架构的核心概念，包括认证/授权、HTTPS/TLS 和加密存储的实现原理。

## 目录结构

```
system-security/
├── main.py    # 安全设计演示代码
├── Makefile   # 运行配置
├── README.md  # 本文件
└── NOTES.md   # 学习笔记
```

## 运行方法

```bash
cd learning/scaffold/grokking/system-security
make run          # 运行演示
```

## 涵盖内容

- JWT 认证与 RBAC 授权
- RBAC 角色权限定义
- HTTPS/TLS 1.3 握手流程
- 加盐哈希密码存储
- 安全最佳实践清单
