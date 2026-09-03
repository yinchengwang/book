# P1 多模态嵌入式 SDK Python 测试
import os
import sys
import pytest
import tempfile

from pymultimodal import DB, Model


@pytest.fixture
def db():
    """创建临时数据库（使用上下文管理器确保关闭）"""
    fd, path = tempfile.mkstemp(suffix=".db")
    os.close(fd)
    db = DB(path)
    yield db, path
    db.close()
    if os.path.exists(path):
        try:
            os.remove(path)
        except PermissionError:
            pass  # Windows 上偶尔会有临时文件占用


class TestDBLifecycle:
    def test_open_close(self):
        fd, path = tempfile.mkstemp(suffix=".db")
        os.close(fd)
        db = DB(path)
        assert db is not None
        db.close()
        if os.path.exists(path):
            os.remove(path)


class TestCollection:
    def test_create_drop(self, db):
        db_obj, _ = db
        db_obj.create_collection("test_coll", Model.VECTOR, vector_dim=128)
        db_obj.drop_collection("test_coll")


class TestVector:
    def test_add_and_search(self, db):
        db_obj, _ = db
        db_obj.create_collection("vec_coll", Model.VECTOR, vector_dim=3)

        ids = ["v1", "v2"]
        embeddings = [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0]]
        db_obj.vectors_add("vec_coll", ids, embeddings)

        results = db_obj.vectors_search("vec_coll", [1.0, 0.0, 0.0], top_k=2)
        assert len(results) > 0
        assert "id" in results[0]
        assert "distance" in results[0]


class TestGraph:
    def test_add_node_edge(self, db):
        db_obj, _ = db
        db_obj.create_collection("graph_coll", Model.GRAPH)

        db_obj.graph_add_node("graph_coll", "n1", "Person", '{"name":"Alice"}')
        db_obj.graph_add_node("graph_coll", "n2", "Person", '{"name":"Bob"}')
        db_obj.graph_add_edge("graph_coll", "n1", "n2", "KNOWS")


class TestTimeseries:
    def test_append_and_query(self, db):
        db_obj, _ = db
        db_obj.create_collection("ts_coll", Model.TIMESERIES)

        db_obj.ts_append("ts_coll", 1000, 3.14, "temp=25")
        db_obj.ts_append("ts_coll", 2000, 6.28, "temp=30")

        results = db_obj.ts_query("ts_coll", 0, 5000)
        assert len(results) == 2
        # 当前 SDK 时序查询结果字段映射简化（distance 当作 timestamp）


class TestText:
    def test_add_and_search(self, db):
        db_obj, _ = db
        db_obj.create_collection("text_coll", Model.TEXT)

        db_obj.text_add("text_coll", "doc1", "hello world")
        db_obj.text_add("text_coll", "doc2", "goodbye world")

        results = db_obj.text_search("text_coll", "hello", top_k=5)
        assert len(results) > 0
        assert results[0]["text"] == "hello world"


class TestContextManager:
    def test_with_statement(self):
        fd, path = tempfile.mkstemp(suffix=".db")
        os.close(fd)
        with DB(path) as db:
            db.create_collection("ctx_coll", Model.VECTOR, vector_dim=4)
        if os.path.exists(path):
            os.remove(path)