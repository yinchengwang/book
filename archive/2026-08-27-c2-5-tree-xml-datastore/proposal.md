# C2-5 Tree 模块 XML 解析与 datastore Proposal

## Why

差距分析 08 卷发现：NETCONF 服务器用字符串扫描做 XML 解析，不支持 XML 属性、命名空间前缀（`netconf_server.c:3-6` 注释明示）——而 NETCONF 所有 RPC 必带 message-id 属性与 base 命名空间，**任何标准 NETCONF 客户端无法接入**；datastore 三态（running/candidate/startup）持久化未确认；Yang XPath 求值缺失；`goto fail` 资源清理多处依赖人工审计（yang_model.c:352,388）。

## What Changes

- 自研 XML 解析器升级（不引入 libxml2）：属性 + 命名空间 + 前缀解析，递归下降 tokenizer
- datastore 三态：running/candidate/startup + candidate → running 原子 commit 校验
- Yang XPath 子集求值器：axis（child/descendant）+ 谓词（[key=value]）
- AST arena 分配器：解析错误一次性释放（替代 goto fail 链）
- YANG import/include 解析 + grouping/uses 展开
- NETCONF 1.1 chunked framing（RFC 6242）
- datastore 接入共享 WAL

## Capabilities

| 能力 | 交付 |
|------|------|
| XML 完整性 | 属性/命名空间/前缀解析单元测试（标准 XML 用例） |
| 标准客户端 | netopeer2/cli 等标准 NETCONF 客户端 hello + get 成功（手工验证记录） |
| datastore | candidate 编辑 + commit 原子生效 + 持久化重启恢复 |
| XPath | YANG 过滤谓词求值正确 |

## Impact

- 修改：netconf_server.c、yang_model.c、yang_engine.c
- 新增：xml_parser.c、yang_xpath.c、datastore.c
- 预计 7-9 个 commit
- 依赖：C0-1、C0-2
