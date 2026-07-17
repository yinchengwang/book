# code_style scaffold

非代码卡——只交付 Makefile + NOTES.md。`make check` 跑 clang-format 干检查；可用 `make fix` 自动修格式。

## 复现（需 clang-format 已安装）

```bash
# 安装
choco install llvm    # Windows
sudo apt install clang-format  # Linux

# 执行
cd learning/scaffold/code_style
make                   # 检查 demo.c 是否违反风格
make fix               # 自动格式化 demo.c
```

## 预期输出

- 如果 clang-format 未安装
```
[style] clang-format 不可用，跳过检查
```
- 如果已安装且 demo.c 格式正确
```
[style] 已安装 clang-format，正在检查 ...
```
- 如果格式错误，make 退出 1 报 `--Werror`

## 关键点

- **本仓库已有 `.clang-format`** 在仓库根目录，基于 clang 生成的编码风格
- **`make check` 是 CI 标准**：`--dry-run --Werror` 结合 `make` exit code 知否违规
- **`.clang-format` 自动发现** 向上树查找到根目录 .clang-format，无显式传参
- **clang-format 的边界**：不处理注释密度、命名规范、API 设计哲学——这属于 human code review 范畴

详见 NOTES.md 工程对照段。
