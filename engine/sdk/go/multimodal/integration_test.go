// 集成测试：需要动态链接 C 库（libmmsdk.a、libsqlite3.a）
// 运行方式：
//
//go:build integration
// +build integration

package multimodal

import (
	"os"
	"path/filepath"
	"testing"
)

// 使用临时目录隔离每个测试，避免 SQLite 锁竞争和集合残留
func tempDBPath(t *testing.T) string {
	dir := t.TempDir()
	return filepath.Join(dir, "test.db")
}

func TestOpenClose(t *testing.T) {
	db, err := Open(tempDBPath(t))
	if err != nil {
		t.Fatalf("Open failed: %v", err)
	}
	if err := db.Close(); err != nil {
		t.Errorf("Close failed: %v", err)
	}
}

func TestCollectionCRUD(t *testing.T) {
	db, err := Open(tempDBPath(t))
	if err != nil {
		t.Fatalf("Open failed: %v", err)
	}
	defer db.Close()

	if err := db.CreateCollection("coll1", ModelVector, 128); err != nil {
		t.Fatalf("CreateCollection failed: %v", err)
	}
	if err := db.DropCollection("coll1"); err != nil {
		t.Errorf("DropCollection failed: %v", err)
	}
}

func TestVectorAddSearch(t *testing.T) {
	db, err := Open(tempDBPath(t))
	if err != nil {
		t.Fatalf("Open failed: %v", err)
	}
	defer db.Close()

	if err := db.CreateCollection("vec", ModelVector, 3); err != nil {
		t.Fatalf("CreateCollection failed: %v", err)
	}

	ids := []string{"v1", "v2"}
	embeddings := [][]float32{
		{1.0, 0.0, 0.0},
		{0.0, 1.0, 0.0},
	}
	if err := db.VectorAdd("vec", ids, embeddings); err != nil {
		t.Fatalf("VectorAdd failed: %v", err)
	}

	hits, err := db.VectorSearch("vec", []float32{1.0, 0.0, 0.0}, 2)
	if err != nil {
		t.Fatalf("VectorSearch failed: %v", err)
	}
	if len(hits) == 0 {
		t.Error("expected non-empty hits")
	}
}

func TestGraph(t *testing.T) {
	db, err := Open(tempDBPath(t))
	if err != nil {
		t.Fatalf("Open failed: %v", err)
	}
	defer db.Close()

	if err := db.CreateCollection("graph", ModelGraph, 0); err != nil {
		t.Fatalf("CreateCollection failed: %v", err)
	}

	if err := db.GraphAddNode("graph", "n1", "Person", `{"name":"Alice"}`); err != nil {
		t.Fatalf("GraphAddNode failed: %v", err)
	}
	if err := db.GraphAddNode("graph", "n2", "Person", `{"name":"Bob"}`); err != nil {
		t.Fatalf("GraphAddNode failed: %v", err)
	}
	if err := db.GraphAddEdge("graph", "n1", "n2", "KNOWS", ""); err != nil {
		t.Fatalf("GraphAddEdge failed: %v", err)
	}
}

func TestTimeseries(t *testing.T) {
	db, err := Open(tempDBPath(t))
	if err != nil {
		t.Fatalf("Open failed: %v", err)
	}
	defer db.Close()

	if err := db.CreateCollection("ts", ModelTimeseries, 0); err != nil {
		t.Fatalf("CreateCollection failed: %v", err)
	}

	if err := db.TSAppend("ts", 1000, 3.14, `temp=25`); err != nil {
		t.Fatalf("TSAppend failed: %v", err)
	}
	if err := db.TSAppend("ts", 2000, 6.28, `temp=30`); err != nil {
		t.Fatalf("TSAppend failed: %v", err)
	}

	points, err := db.TSQuery("ts", 0, 5000)
	if err != nil {
		t.Fatalf("TSQuery failed: %v", err)
	}
	if len(points) != 2 {
		t.Errorf("expected 2 points, got %d", len(points))
	}
}

func TestText(t *testing.T) {
	db, err := Open(tempDBPath(t))
	if err != nil {
		t.Fatalf("Open failed: %v", err)
	}
	defer db.Close()

	if err := db.CreateCollection("text", ModelText, 0); err != nil {
		t.Fatalf("CreateCollection failed: %v", err)
	}

	if err := db.TextAdd("text", "doc1", "hello world", ""); err != nil {
		t.Fatalf("TextAdd failed: %v", err)
	}
	if err := db.TextAdd("text", "doc2", "goodbye world", ""); err != nil {
		t.Fatalf("TextAdd failed: %v", err)
	}

	hits, err := db.TextSearch("text", "hello", 5)
	if err != nil {
		t.Fatalf("TextSearch failed: %v", err)
	}
	if len(hits) == 0 {
		t.Error("expected non-empty hits")
	}
}

// 保留旧函数以兼容可能引用了 cleanupDB 的扩展
var _ = func(t *testing.T, path string) {
	_ = os.Remove(path)
}