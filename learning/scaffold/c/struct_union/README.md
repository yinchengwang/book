# struct_union scaffold

struct layout + 字节对齐 + bit field + union + flexible array member。

## 复现

```bash
cd learning/scaffold/struct_union
gcc -Wall -Wextra -O2 -std=c11 -o struct_demo main.c
./struct_demo
```

## 预期输出

```
[struct]   offset(x)=0 offset(y)=4 offset(z)=8 sizeof=12
[packed]   sizeof=6
[bitfld]   a=1 b=5 c=10  (sizeof=4)
[union]    sizeof=4, pi as int=0x40490FDB, as float=3.141593
[flex]     len=16 data='hello-flex-array'
```

## 关键点

- **struct 默认有 padding**：让 int 等基础类型对齐 4 字节 → sizeof 常常大于字段宽度之和
- **`__attribute__((packed))`** 取消 padding：节省内存但访问速度低（未对齐访存）
- **`offsetof(struct, field)`** 拿字段偏移；与 `assert(offsetof == X)` 一起是抗 ABI 漂移的标配
- **bit field**：节省内存但编译器实现差异大——非网络协议或二进制格式慎用
- **union**："同一份内存多种解释"，type punning 利器；写时一端、读时另一端
- **flexible array member** (`char data[]`)：变长结构体，malloc 时按 `sizeof(struct packet) + extra` 分配；这是 PG WAL 页头、protobuf 序列化常见手法

详见 NOTES.md 工程对照段。
