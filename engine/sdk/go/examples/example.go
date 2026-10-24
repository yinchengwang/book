// 多模态 SDK Go 绑定示例程序
package main

import (
	"fmt"
	"log"
	"os"

	"github.com/multimodal/multimodal"
)

func main() {
	if len(os.Args) < 2 {
		fmt.Println("用法: example <db_path>")
		os.Exit(1)
	}
	dbPath := os.Args[1]

	db, err := multimodal.Open(dbPath)
	if err != nil {
		log.Fatalf("打开数据库失败: %v", err)
	}
	defer db.Close()

	fmt.Println("=== 向量模型示例 ===")
	if err := db.CreateCollection("embeddings", multimodal.ModelVector, 128); err != nil {
		log.Printf("CreateCollection 失败: %v", err)
	}

	if err := db.VectorAdd("embeddings",
		[]string{"doc1", "doc2"},
		[][]float32{
			{0.1, 0.2, 0.3},
			{0.4, 0.5, 0.6},
		}); err != nil {
		log.Printf("VectorAdd 失败: %v", err)
	}

	hits, err := db.VectorSearch("embeddings", []float32{0.1, 0.2, 0.3}, 5)
	if err != nil {
		log.Printf("VectorSearch 失败: %v", err)
	} else {
		for _, hit := range hits {
			fmt.Printf("  id=%s distance=%.4f\n", hit.ID, hit.Distance)
		}
	}

	fmt.Println("=== 图模型示例 ===")
	if err := db.CreateCollection("social", multimodal.ModelGraph, 0); err != nil {
		log.Printf("CreateCollection 失败: %v", err)
	}

	if err := db.GraphAddNode("social", "alice", "Person", `{"age":30}`); err != nil {
		log.Printf("GraphAddNode 失败: %v", err)
	}
	if err := db.GraphAddNode("social", "bob", "Person", `{"age":25}`); err != nil {
		log.Printf("GraphAddNode 失败: %v", err)
	}
	if err := db.GraphAddEdge("social", "alice", "bob", "KNOWS", ""); err != nil {
		log.Printf("GraphAddEdge 失败: %v", err)
	}

	fmt.Println("=== 时序模型示例 ===")
	if err := db.CreateCollection("metrics", multimodal.ModelTimeseries, 0); err != nil {
		log.Printf("CreateCollection 失败: %v", err)
	}
	if err := db.TSAppend("metrics", 1000, 3.14, `host=server1`); err != nil {
		log.Printf("TSAppend 失败: %v", err)
	}
	points, err := db.TSQuery("metrics", 0, 5000)
	if err != nil {
		log.Printf("TSQuery 失败: %v", err)
	} else {
		fmt.Printf("  共 %d 个数据点\n", len(points))
	}

	fmt.Println("=== 文本模型示例 ===")
	if err := db.CreateCollection("docs", multimodal.ModelText, 0); err != nil {
		log.Printf("CreateCollection 失败: %v", err)
	}
	if err := db.TextAdd("docs", "doc1", "hello world", ""); err != nil {
		log.Printf("TextAdd 失败: %v", err)
	}
	hits, err := db.TextSearch("docs", "hello", 5)
	if err != nil {
		log.Printf("TextSearch 失败: %v", err)
	} else {
		for _, hit := range hits {
			fmt.Printf("  id=%s text=%q\n", hit.ID, hit.Text)
		}
	}
}