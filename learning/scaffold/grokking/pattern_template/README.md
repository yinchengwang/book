# 模板方法模式 (Template Method Pattern)

## 简介

模板方法模式在父类中定义算法骨架, 子类实现具体步骤。本例展示了
三种实际场景: 数据迁移框架、饮料制作流程、测试框架模拟。

## 目录结构

```
pattern_template/
├── main.py      # 模板方法演示代码
├── Makefile     # 构建和运行配置
├── README.md    # 本文件
└── NOTES.md     # 学习笔记
```

## 运行方式

```bash
make run
# 或直接运行:
python3 main.py
```

## 示例要点

1. 数据迁移框架: 抽取-转换-加载 (ETL) 的通用骨架
2. 饮料制作: 钩子方法控制是否添加调料
3. 测试框架: junit 风格的 setup-test-teardown 骨架
