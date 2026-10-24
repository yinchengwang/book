# pymultimodal — P1 多模态嵌入式 SDK Python 绑定

通过 pybind11 封装 C ABI，提供 Pythonic 接口访问向量 / 图 / 时序 / 文本四种数据模型。

## 平台要求

- Windows 平台需安装 MinGW GCC（`C:\mingw64\`），用于编译 C++ 扩展
- Python 3.9+
- pybind11（`pip install pybind11`）

## 安装

```bash
# 1. 先用 CMake 构建 C 库
cmake -S . -B build/engineering -G Ninja -DCMAKE_BUILD_TYPE=Release -DENGINEERING_BUILD=ON
cmake --build build/engineering --target mmsdk

# 2. 安装 Python 包
pip install .

# 3. 复制 MinGW 运行时 DLL 到包目录
cp /c/mingw64/bin/{libgcc_s_seh-1,libstdc++-6,libwinpthread-1}.dll pymultimodal/
```

## 使用示例

```python
from pymultimodal import DB, Model

# 创建 / 打开数据库
db = DB("my_data.db")
try:
    # 创建向量集合（需要指定维度）
    db.create_collection("embeddings", Model.VECTOR, vector_dim=128)

    # 批量添加向量
    db.vectors_add(
        "embeddings",
        ids=["doc1", "doc2"],
        embeddings=[[0.1, 0.2, ...], [0.3, 0.4, ...]]
    )

    # 向量搜索
    results = db.vectors_search("embeddings", query=[0.1, 0.2, ...], top_k=10)
    for hit in results:
        print(hit["id"], hit["distance"])

    # 也支持上下文管理器
    with DB("other.db") as db:
        db.create_collection("graph", Model.GRAPH)
        db.graph_add_node("graph", "alice", "Person")
        db.graph_add_node("graph", "bob", "Person")
        db.graph_add_edge("graph", "alice", "bob", "KNOWS")
finally:
    db.close()
```

## 测试

```bash
pytest tests/ -v
```

## 已知限制

- 时序查询的 `timestamp` / `value` 字段当前简化映射（通过 metadata_json 编码）
- text_search 结果的 `score` 实际为距离
- 图遍历 / 全文搜索的高级功能（path 查询、FTS5 排序）暂未在 Python 接口暴露

## 注意事项

Windows 下 MinGW 编译的 `_core.pyd` 依赖 MinGW 运行时 DLL。三种解决方案：
1. 复制 DLL 到 `pymultimodal/` 同目录
2. 将 `C:\mingw64\bin` 加入系统 PATH
3. 改用 MSVC 编译器（需安装 Visual Studio Build Tools）