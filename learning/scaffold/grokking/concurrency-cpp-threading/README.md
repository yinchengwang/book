# C++ 多线程基础 (C++17 Threading)

## 简介

C++17 标准多线程库演示 —— thread / future & promise / packaged_task / async。

## 目录结构

```
concurrency-cpp-threading/
├── main.cpp   # 演示代码
├── Makefile   # 构建配置
├── README.md  # 本文件
└── NOTES.md   # 学习笔记
```

## 运行方法

```bash
cd learning/scaffold/grokking/concurrency-cpp-threading
make run

# 或手动编译运行
g++ -std=c++17 -Wall -Wextra -pthread -o main main.cpp
./main
```

## 涵盖内容

- `std::thread` — 基本线程创建与 join
- `std::future` & `std::promise` — 跨线程返回值
- `std::packaged_task` — 包装可调用对象为异步任务
- `std::async` — 高级异步任务启动
