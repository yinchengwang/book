"""
nosql — NoSQL 数据库分类演示

演示 KV 存储 / 文档数据库 / 列族存储 / 图数据库的核心特性和使用场景。
"""

from typing import Dict, List, Any, Optional, Set, Tuple
from dataclasses import dataclass, field
import json


# ════════════════════════════════════════════════════════════
# 1. KV 存储 (Key-Value Store)
# ════════════════════════════════════════════════════════════
class KVStore:
    """模拟 Redis / Memcached 风格的 KV 存储"""

    def __init__(self):
        self.store: Dict[str, str] = {}
        self.ttl: Dict[str, float] = {}

    def set(self, key: str, value: str):
        self.store[key] = value
        print(f"  [KV] SET {key} = {value}")

    def get(self, key: str) -> Optional[str]:
        val = self.store.get(key)
        print(f"  [KV] GET {key} → {val}")
        return val

    def delete(self, key: str):
        if key in self.store:
            del self.store[key]
            print(f"  [KV] DEL {key}")

    def scan_prefix(self, prefix: str) -> List[str]:
        return [k for k in self.store if k.startswith(prefix)]


def demo_kv():
    print("═══ 1. KV 存储（Redis） ═══")
    kv = KVStore()
    kv.set("session:user:1001", json.dumps(
        {"name": "Alice", "role": "admin"}))
    kv.set("session:user:1002", json.dumps(
        {"name": "Bob", "role": "user"}))
    kv.set("cache:hot_products", json.dumps(["p1", "p2", "p3"]))
    kv.get("session:user:1001")
    kv.delete("session:user:1002")
    print(f"  前缀扫描 session:user: → {kv.scan_prefix('session:user:')}")
    print("  适用场景：缓存、会话管理、计数器\n")


# ════════════════════════════════════════════════════════════
# 2. 文档数据库 (Document Store)
# ════════════════════════════════════════════════════════════
@dataclass
class Document:
    """模拟 MongoDB 风格的文档"""
    _id: str
    data: Dict[str, Any]


class DocDB:
    def __init__(self):
        self.collections: Dict[str, Dict[str, Document]] = {}

    def insert(self, collection: str, doc: Dict[str, Any]) -> str:
        doc_id = doc.get("_id", str(hash(str(doc))))
        if collection not in self.collections:
            self.collections[collection] = {}
        self.collections[collection][doc_id] = Document(
            _id=doc_id, data=doc)
        print(f"  [DocDB] 插入 {collection}/{doc_id}")
        return doc_id

    def query(self, collection: str,
              filter_fn=lambda d: True) -> List[Document]:
        docs = list(self.collections.get(collection, {}).values())
        return [d for d in docs if filter_fn(d.data)]

    def update(self, collection: str, doc_id: str,
               update: Dict[str, Any]) -> bool:
        if collection in self.collections and doc_id in self.collections[collection]:
            self.collections[collection][doc_id].data.update(update)
            return True
        return False


def demo_doc():
    print("═══ 2. 文档数据库（MongoDB） ═══")
    db = DocDB()
    db.insert("users", {
        "_id": "u001", "name": "Alice", "age": 30,
        "address": {"city": "北京", "zip": "100000"},
        "tags": ["工程师", "后端"]
    })
    db.insert("users", {
        "_id": "u002", "name": "Bob", "age": 25,
        "address": {"city": "上海", "zip": "200000"},
        "tags": ["产品经理"]
    })
    db.insert("orders", {
        "_id": "o001", "user_id": "u001",
        "items": [{"product": "MacBook", "price": 12999}],
        "total": 12999
    })

    alice = db.query("users", lambda d: d.get("name") == "Alice")
    print(f"  查询 Alice: {len(alice)} 条")

    young = db.query("users", lambda d: d.get("age", 0) < 30)
    print(f"  年龄 < 30: {len(young)} 条")

    print("  适用场景：内容管理、日志、Web 应用\n")


# ════════════════════════════════════════════════════════════
# 3. 列族存储 (Column Family Store)
# ════════════════════════════════════════════════════════════
class ColumnFamily:
    """模拟 HBase / Cassandra 风格的列族存储"""

    def __init__(self, name: str, families: List[str]):
        self.name = name
        self.families = families
        # row_key → cf_name → col_name → value
        self.data: Dict[str, Dict[str, Dict[str, str]]] = {}

    def put(self, row_key: str, cf: str, col: str, value: str):
        if row_key not in self.data:
            self.data[row_key] = {f: {} for f in self.families}
        self.data[row_key][cf][col] = value

    def get(self, row_key: str, cf: str) -> Dict[str, str]:
        return self.data.get(row_key, {}).get(cf, {})

    def scan(self) -> List[Tuple[str, Dict]]:
        return [(k, v) for k, v in self.data.items()]


def demo_column():
    print("═══ 3. 列族存储（HBase/Cassandra） ═══")
    cf = ColumnFamily("user_profile", ["info", "activity", "prefs"])

    cf.put("user_1001", "info", "name", "Alice")
    cf.put("user_1001", "info", "email", "alice@test.com")
    cf.put("user_1001", "activity", "last_login", "2026-07-12")
    cf.put("user_1001", "activity", "login_count", "42")
    cf.put("user_1001", "prefs", "theme", "dark")

    cf.put("user_1002", "info", "name", "Bob")
    cf.put("user_1002", "activity", "last_login", "2026-07-11")
    cf.put("user_1002", "activity", "login_count", "18")

    info = cf.get("user_1001", "info")
    print(f"  user_1001/info: {info}")
    activity = cf.get("user_1001", "activity")
    print(f"  user_1001/activity: {activity}")

    print("  适用场景：时序数据、宽表、事件溯源\n")


# ════════════════════════════════════════════════════════════
# 4. 图数据库 (Graph Database)
# ════════════════════════════════════════════════════════════
class GraphDB:
    """模拟 Neo4j 风格的图数据库"""

    def __init__(self):
        self.nodes: Dict[str, Dict] = {}
        self.edges: List[Tuple[str, str, str, Dict]] = []

    def add_node(self, node_id: str, label: str, props: Dict):
        self.nodes[node_id] = {"label": label, "props": props}

    def add_edge(self, from_id: str, to_id: str,
                 rel_type: str, props: Dict = None):
        self.edges.append((from_id, to_id, rel_type, props or {}))

    def shortest_path(self, start: str, end: str) -> Optional[List[str]]:
        """BFS 找最短路径"""
        visited = {start}
        queue = [[start]]
        while queue:
            path = queue.pop(0)
            node = path[-1]
            for f, t, rt, _ in self.edges:
                neighbor = t if f == node else (f if t == node else None)
                if neighbor and neighbor not in visited:
                    visited.add(neighbor)
                    new_path = path + [neighbor]
                    if neighbor == end:
                        return new_path
                    queue.append(new_path)
        return None


def demo_graph():
    print("═══ 4. 图数据库（Neo4j） ═══")
    g = GraphDB()
    g.add_node("1", "Person", {"name": "Alice", "age": 30})
    g.add_node("2", "Person", {"name": "Bob", "age": 28})
    g.add_node("3", "Company", {"name": "Acme Inc"})
    g.add_node("4", "Person", {"name": "Charlie", "age": 35})
    g.add_node("5", "City", {"name": "北京"})

    g.add_edge("1", "2", "KNOWS", {"since": 2010})
    g.add_edge("2", "4", "KNOWS", {"since": 2015})
    g.add_edge("1", "3", "WORKS_AT", {"role": "工程师"})
    g.add_edge("4", "3", "WORKS_AT", {"role": "CTO"})
    g.add_edge("1", "5", "LIVES_IN", {})
    g.add_edge("4", "5", "LIVES_IN", {})

    path = g.shortest_path("1", "4")
    print(f"  Alice → Charlie 最短路径: {' → '.join(path) if path else '无'}")
    print(f"  节点 {len(g.nodes)} 个，关系 {len(g.edges)} 条")
    print("  适用场景：社交网络、推荐引擎、知识图谱\n")


def main():
    print("=" * 50)
    print("  NoSQL 演示 — KV / 文档 / 列族 / 图")
    print("=" * 50 + "\n")

    demo_kv()
    demo_doc()
    demo_column()
    demo_graph()

    print("所有 NoSQL 演示完成。")


if __name__ == "__main__":
    main()
