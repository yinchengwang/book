# python-testing 学习笔记

## 概念地图

Python 测试框架：

- **unittest**：标准库，基于 Java JUnit
- **pytest**：社区主流，更简洁强大
- **Mock**：模拟外部依赖
- **patch**：临时替换模块属性

## 测试原则

1. **Arrange-Act-Assert**：准备→执行→验证
2. **独立性**：每个测试独立运行
3. **可重复**：多次运行结果一致
4. **快速**：避免真正的 I/O 操作

## Mock 使用场景

1. **外部 API**：模拟网络请求
2. **数据库**：模拟数据库操作
3. **文件系统**：模拟文件读写
4. **时间**：mock datetime.now()

## 工程对照（≥100 字硬约束）

测试在工程实践中非常重要：

1. **TDD**：测试驱动开发
2. **CI/CD**：自动化测试
3. **覆盖率**：pytest-cov 测量
4. **fixtures**：pytest fixtures 复用设置
5. **parametrize**：参数化测试
6. **mock**：isolate 待测代码

学完本卡能动手的事：为项目编写测试用例，练习 TDD。
