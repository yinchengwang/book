# C2-5 Tree XML 与 datastore 设计文档

## 设计目标

修复 08 卷识别的 Tree 模态缺陷：NETCONF 服务器无法处理标准 XML（不支持属性/命名空间），datastore 三态未实装，XPath 缺失，goto fail 资源清理脆弱。

## 方案

### 1. 自研 XML 解析器（T1）

递归下降 tokenizer（不引入 libxml2）：
- 元素开始标签（含属性 `name="value"` 与 `xmlns:prefix="uri"` 命名空间声明）
- 元素结束标签 `</name>`
- 文本内容
- 自闭合 `<elem/>`
- 注释 `<!-- -->` 跳过

### 2. AST arena（T8）

单一 malloc 大块存放所有节点 + 字符串；错误或 reset 时一次 free。

### 3. datastore 三态（T4-T6）

- `running`：当前生效（读默认）
- `candidate`：编辑中（写默认）
- `startup`：启动时加载
- candidate → running：原子切换（write 文件 + rename 临时文件）

### 4. Yang XPath 子集（T7）

支持 axis：`child`/`descendant` + 谓词 `[key='value']` + 节点名匹配。
