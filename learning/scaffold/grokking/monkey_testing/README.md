# 混沌测试/故障注入

## 目录结构

```
monkey_testing/
├── main.py     # 混沌测试演示代码
├── Makefile    # 构建和运行脚本
└── README.md   # 本文件
```

## 功能说明

模拟一个微服务调用链（api-gateway → user-service → order-service → payment-service → inventory-service），演示：

- **混沌工程 (Chaos Engineering)**: 在生产环境中注入故障，验证系统韧性
- **故障注入 (Fault Injection)**: 模拟网络延迟、服务崩溃、资源耗尽
- **熔断器 (Circuit Breaker)**: 三态（closed/open/half_open）保护系统
- **指数退避重试 (Retry with Backoff)**: 避免重试风暴

## 五个实验场景

1. 所有服务正常
2. 单个服务高延迟
3. 故障注入触发熔断
4. 服务完全宕机
5. 级联故障（多个服务同时异常）

## 如何运行

```bash
cd learning/scaffold/grokking/monkey_testing/
make run
# 或直接
python3 main.py
```
