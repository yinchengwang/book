# 工厂模式

## 描述

本模块演示了三种工厂模式: 简单工厂 (Simple Factory)、工厂方法 (Factory Method) 和抽象工厂 (Abstract Factory), 以及它们的递进关系。

本演示涵盖:
- **简单工厂**: 支付系统示例 (支付宝/微信/信用卡), 集中式对象创建
- **工厂方法**: 文档编辑应用示例 (PDF/Word/电子表格), 延迟实例化到子类
- **抽象工厂**: 跨平台 UI 组件示例 (Windows/macOS/Linux), 创建产品家族
- **数据库连接工厂**: 简单工厂 + 注册机制的动态扩展示例

## 目录结构

```
pattern_factory/
├── main.py    # 主演示文件
├── Makefile   # 构建/运行配置
├── README.md  # 本文件
└── NOTES.md   # 学习笔记
```

## 如何运行

```bash
# 运行演示
make run

# 或者直接使用 python3
python3 main.py

# 检查环境并运行
make check
```
