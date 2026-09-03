/**
 * @file multimodal.go
 * @brief P1 多模态嵌入式 SDK Go 绑定（基于 cgo）
 *
 * 通过 cgo 调用 C ABI，封装向量 / 图 / 时序 / 文本四种模型。
 *
 * 构建要求：
 *  1. 已通过 CMake 构建 mmsdk 静态库（libmmsdk.a / libsqlite3.a）
 *  2. CGO 编译时需指定头文件路径和库路径：
 *     CGO_CFLAGS="-I/path/to/include -I/path/to/third_part/sqlite3"
 *     CGO_LDFLAGS="-L/path/to/build/engineering/src/sdk -L/path/to/build/engineering/sdk_sqlite3_build -lmmsdk -lsqlite3"
 */
package multimodal

/*
#include <stdlib.h>
#include <string.h>

#include "sdk/mmdb.h"
#include "sdk/mmdb_types.h"
#include "sdk/mmdb_vectors.h"
#include "sdk/mmdb_graph.h"
#include "sdk/mmdb_timeseries.h"
#include "sdk/mmdb_text.h"
#include "sdk/mmdb_error.h"

// 辅助函数：把 Go 字符串转 C 字符串
static char* go_str_to_c(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* out = (char*)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len + 1);
    return out;
}
*/
import "C"

import (
	"errors"
	"fmt"
	"runtime"
	"unsafe"
)

// Model 表示数据模型类型
type Model int

const (
	ModelVector     Model = 0
	ModelGraph      Model = 1
	ModelTimeseries Model = 2
	ModelText       Model = 3
)

// String 返回模型名称
func (m Model) String() string {
	switch m {
	case ModelVector:
		return "vector"
	case ModelGraph:
		return "graph"
	case ModelTimeseries:
		return "timeseries"
	case ModelText:
		return "text"
	default:
		return "unknown"
	}
}

// Hit 表示搜索结果项
type Hit struct {
	ID       string
	Distance float32
	Text     string
	Metadata string
}

// DB 封装了 mmdb_t 句柄，是所有操作的入口
type DB struct {
	cptr *C.mmdb_t
}

// Open 打开或创建数据库
func Open(path string) (*DB, error) {
	cPath := C.CString(path)
	defer C.free(unsafe.Pointer(cPath))

	handle := C.mmdb_open(cPath, nil)
	if handle == nil {
		return nil, errors.New("failed to open database: " + path)
	}

	db := &DB{cptr: handle}
	runtime.SetFinalizer(db, (*DB).Close)
	return db, nil
}

// Close 关闭数据库
func (db *DB) Close() error {
	if db.cptr != nil {
		C.mmdb_close(db.cptr)
		db.cptr = nil
	}
	return nil
}

// LastError 返回最近一次错误码
func (db *DB) LastError() (int, string) {
	if db.cptr == nil {
		return -1, "db closed"
	}
	code := int(C.mmdb_last_error_code(db.cptr))
	msg := C.mmdb_last_error_message(db.cptr)
	if msg == nil {
		return code, ""
	}
	return code, C.GoString(msg)
}

// checkRC 检查返回码，失败时返回 error
func (db *DB) checkRC(rc C.int) error {
	if rc == C.MMDB_OK {
		return nil
	}
	code, msg := db.LastError()
	if msg == "" {
		msg = C.GoString(C.mmdb_strerror(rc))
	}
	return fmt.Errorf("mmdb error (code=%d): %s", code, msg)
}

// CreateCollection 创建集合（指定模型类型）
//
// 对于 vector 模型必须指定 vectorDim；其它模型传 0 即可。
func (db *DB) CreateCollection(name string, model Model, vectorDim uint32) error {
	if db.cptr == nil {
		return errors.New("db closed")
	}

	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	schema := C.mmdb_schema_t{
		model:      C.mmdb_model_t(model),
		field_count: 0,
		fields:     nil,
		vector_dim: C.size_t(vectorDim),
	}

	c := C.mmdb_collection_create(db.cptr, cName, &schema)
	if c == nil {
		return db.checkRC(C.mmdb_last_error_code(db.cptr))
	}
	// c 是借用指针，无需释放
	return nil
}

// DropCollection 删除集合
func (db *DB) DropCollection(name string) error {
	if db.cptr == nil {
		return errors.New("db closed")
	}

	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	c := C.mmdb_collection_get(db.cptr, cName)
	if c == nil {
		return fmt.Errorf("collection not found: %s", name)
	}
	// mmdb_collection_drop 返回 void，会释放 c
	C.mmdb_collection_drop(c)
	return nil
}

// ========================================================================
// 向量操作
// ========================================================================

// VectorAdd 批量添加向量
func (db *DB) VectorAdd(collection string, ids []string, embeddings [][]float32) error {
	if len(ids) != len(embeddings) {
		return errors.New("ids and embeddings must have same length")
	}
	if db.cptr == nil {
		return errors.New("db closed")
	}
	if len(ids) == 0 {
		return nil
	}

	cColl := C.CString(collection)
	defer C.free(unsafe.Pointer(cColl))

	c := C.mmdb_collection_get(db.cptr, cColl)
	if c == nil {
		return fmt.Errorf("collection not found: %s", collection)
	}

	dim := len(embeddings[0])

	// 将所有 embedding 复制到 C 内存（避免 cgo 指针规则限制）
	totalFloats := len(embeddings) * dim
	cFloats := C.malloc(C.size_t(totalFloats) * C.size_t(unsafe.Sizeof(C.float(0))))
	if cFloats == nil {
		return errors.New("malloc failed")
	}
	defer C.free(cFloats)

	floatSlice := unsafe.Slice((*C.float)(cFloats), totalFloats)
	for i, emb := range embeddings {
		for j, v := range emb {
			floatSlice[i*dim+j] = C.float(v)
		}
	}

	// 构造 C 数组
	cVecs := make([]C.mmdb_vector_t, len(ids))
	idCStrs := make([]*C.char, len(ids))
	for i, id := range ids {
		idC := C.CString(id)
		idCStrs[i] = idC
		cVecs[i].id = (*C.uchar)(unsafe.Pointer(idC))
		cVecs[i].id_len = C.size_t(len(id))
		cVecs[i].vector = (*C.float)(unsafe.Pointer(&floatSlice[i*dim]))
		cVecs[i].dim = C.size_t(dim)
		cVecs[i].metadata_json = nil
		cVecs[i].text = nil
	}

	rc := C.mmdb_vectors_add(c, &cVecs[0], C.size_t(len(cVecs)))
	for _, p := range idCStrs {
		C.free(unsafe.Pointer(p))
	}
	return db.checkRC(rc)
}

// VectorSearch 向量搜索
func (db *DB) VectorSearch(collection string, query []float32, topK int) ([]Hit, error) {
	if db.cptr == nil {
		return nil, errors.New("db closed")
	}

	cColl := C.CString(collection)
	defer C.free(unsafe.Pointer(cColl))

	c := C.mmdb_collection_get(db.cptr, cColl)
	if c == nil {
		return nil, fmt.Errorf("collection not found: %s", collection)
	}

	// 将 query 复制到 C 内存
	cQueryBuf := C.malloc(C.size_t(len(query)) * C.size_t(unsafe.Sizeof(C.float(0))))
	if cQueryBuf == nil {
		return nil, errors.New("malloc failed")
	}
	defer C.free(cQueryBuf)
	querySlice := unsafe.Slice((*C.float)(cQueryBuf), len(query))
	for i, v := range query {
		querySlice[i] = C.float(v)
	}

	q := C.mmdb_query_t{
		query_vector: (*C.float)(cQueryBuf),
		dim:          C.size_t(len(query)),
		top_k:        C.size_t(topK),
		filter_json:  nil,
	}

	var result C.mmdb_result_t
	rc := C.mmdb_vectors_search(c, &q, &result)
	if err := db.checkRC(rc); err != nil {
		return nil, err
	}
	defer C.mmdb_result_free(&result)

	hits := make([]Hit, 0, result.count)
	for i := C.size_t(0); i < result.count; i++ {
		item := (*C.mmdb_result_item_t)(unsafe.Pointer(
			uintptr(unsafe.Pointer(result.items)) + uintptr(i)*unsafe.Sizeof(*result.items),
		))
		hit := Hit{
			Distance: float32(item.distance),
		}
		if item.id != nil && item.id_len > 0 {
			hit.ID = C.GoStringN((*C.char)(unsafe.Pointer(item.id)), C.int(item.id_len))
		}
		if item.metadata_json != nil {
			hit.Metadata = C.GoString(item.metadata_json)
		}
		if item.text != nil {
			hit.Text = C.GoString(item.text)
		}
		hits = append(hits, hit)
	}
	return hits, nil
}

// ========================================================================
// 图操作
// ========================================================================

// GraphAddNode 添加图节点
func (db *DB) GraphAddNode(collection, id, label string, propertiesJSON string) error {
	if db.cptr == nil {
		return errors.New("db closed")
	}

	cColl := C.CString(collection)
	defer C.free(unsafe.Pointer(cColl))

	c := C.mmdb_collection_get(db.cptr, cColl)
	if c == nil {
		return fmt.Errorf("collection not found: %s", collection)
	}

	cID := C.CString(id)
	defer C.free(unsafe.Pointer(cID))

	var cLabel, cProps *C.char
	if label != "" {
		cLabel = C.CString(label)
		defer C.free(unsafe.Pointer(cLabel))
	}
	if propertiesJSON != "" {
		cProps = C.CString(propertiesJSON)
		defer C.free(unsafe.Pointer(cProps))
	}

	node := C.mmdb_node_t{
		id:              cID,
		label:           cLabel,
		properties_json: cProps,
	}

	rc := C.mmdb_graph_add_node(c, &node)
	return db.checkRC(rc)
}

// GraphAddEdge 添加图边
func (db *DB) GraphAddEdge(collection, srcID, dstID, label string, propertiesJSON string) error {
	if db.cptr == nil {
		return errors.New("db closed")
	}

	cColl := C.CString(collection)
	defer C.free(unsafe.Pointer(cColl))

	c := C.mmdb_collection_get(db.cptr, cColl)
	if c == nil {
		return fmt.Errorf("collection not found: %s", collection)
	}

	cSrc := C.CString(srcID)
	defer C.free(unsafe.Pointer(cSrc))

	cDst := C.CString(dstID)
	defer C.free(unsafe.Pointer(cDst))

	var cLabel, cProps *C.char
	if label != "" {
		cLabel = C.CString(label)
		defer C.free(unsafe.Pointer(cLabel))
	}
	if propertiesJSON != "" {
		cProps = C.CString(propertiesJSON)
		defer C.free(unsafe.Pointer(cProps))
	}

	edge := C.mmdb_edge_t{
		source_id:       cSrc,
		target_id:       cDst,
		label:           cLabel,
		weight:          1.0,
		properties_json: cProps,
	}

	rc := C.mmdb_graph_add_edge(c, &edge)
	return db.checkRC(rc)
}

// ========================================================================
// 时序操作
// ========================================================================

// TSAppend 追加时序数据点
func (db *DB) TSAppend(collection string, timestamp int64, value float64, tagsJSON string) error {
	if db.cptr == nil {
		return errors.New("db closed")
	}

	cColl := C.CString(collection)
	defer C.free(unsafe.Pointer(cColl))

	c := C.mmdb_collection_get(db.cptr, cColl)
	if c == nil {
		return fmt.Errorf("collection not found: %s", collection)
	}

	var cTags *C.char
	if tagsJSON != "" {
		cTags = C.CString(tagsJSON)
		defer C.free(unsafe.Pointer(cTags))
	}

	dp := C.mmdb_datapoint_t{
		timestamp: C.int64_t(timestamp),
		value:     C.double(value),
		tags_json: cTags,
	}

	rc := C.mmdb_timeseries_append(c, &dp)
	return db.checkRC(rc)
}

// TSQuery 时序数据查询
//
// 注意：当前 C SDK 时序查询结果通过 mmdb_result_t 通用结构返回，
// timestamp/value 通过距离/元数据字段间接编码（简化映射）。
func (db *DB) TSQuery(collection string, startTS, endTS int64) ([]map[string]interface{}, error) {
	if db.cptr == nil {
		return nil, errors.New("db closed")
	}

	cColl := C.CString(collection)
	defer C.free(unsafe.Pointer(cColl))

	c := C.mmdb_collection_get(db.cptr, cColl)
	if c == nil {
		return nil, fmt.Errorf("collection not found: %s", collection)
	}

	q := C.mmdb_ts_query_t{
		start: C.int64_t(startTS),
		end:   C.int64_t(endTS),
	}

	var result C.mmdb_result_t
	rc := C.mmdb_timeseries_query(c, &q, &result)
	if err := db.checkRC(rc); err != nil {
		return nil, err
	}
	defer C.mmdb_result_free(&result)

	points := make([]map[string]interface{}, 0, result.count)
	for i := C.size_t(0); i < result.count; i++ {
		item := (*C.mmdb_result_item_t)(unsafe.Pointer(
			uintptr(unsafe.Pointer(result.items)) + uintptr(i)*unsafe.Sizeof(*result.items),
		))
		point := map[string]interface{}{}
		point["timestamp"] = float64(item.distance) // 简化映射
		point["value"] = 0.0
		if item.metadata_json != nil {
			point["tags"] = C.GoString(item.metadata_json)
		}
		points = append(points, point)
	}
	return points, nil
}

// ========================================================================
// 文本操作
// ========================================================================

// TextAdd 添加文本条目
func (db *DB) TextAdd(collection, id, text string, metadataJSON string) error {
	if db.cptr == nil {
		return errors.New("db closed")
	}

	cColl := C.CString(collection)
	defer C.free(unsafe.Pointer(cColl))

	c := C.mmdb_collection_get(db.cptr, cColl)
	if c == nil {
		return fmt.Errorf("collection not found: %s", collection)
	}

	cID := C.CString(id)
	defer C.free(unsafe.Pointer(cID))

	cText := C.CString(text)
	defer C.free(unsafe.Pointer(cText))

	var cMeta *C.char
	if metadataJSON != "" {
		cMeta = C.CString(metadataJSON)
		defer C.free(unsafe.Pointer(cMeta))
	}

	entry := C.mmdb_text_entry_t{
		id:            cID,
		text:          cText,
		metadata_json: cMeta,
	}

	rc := C.mmdb_text_add(c, &entry)
	return db.checkRC(rc)
}

// TextSearch 全文搜索
func (db *DB) TextSearch(collection, query string, topK int) ([]Hit, error) {
	if db.cptr == nil {
		return nil, errors.New("db closed")
	}

	cColl := C.CString(collection)
	defer C.free(unsafe.Pointer(cColl))

	c := C.mmdb_collection_get(db.cptr, cColl)
	if c == nil {
		return nil, fmt.Errorf("collection not found: %s", collection)
	}

	cQuery := C.CString(query)
	defer C.free(unsafe.Pointer(cQuery))

	q := C.mmdb_text_query_t{
		query: cQuery,
		top_k: C.size_t(topK),
	}

	var result C.mmdb_result_t
	rc := C.mmdb_text_search(c, &q, &result)
	if err := db.checkRC(rc); err != nil {
		return nil, err
	}
	defer C.mmdb_result_free(&result)

	hits := make([]Hit, 0, result.count)
	for i := C.size_t(0); i < result.count; i++ {
		item := (*C.mmdb_result_item_t)(unsafe.Pointer(
			uintptr(unsafe.Pointer(result.items)) + uintptr(i)*unsafe.Sizeof(*result.items),
		))
		hit := Hit{
			Distance: float32(item.distance),
		}
		if item.id != nil && item.id_len > 0 {
			hit.ID = C.GoStringN((*C.char)(unsafe.Pointer(item.id)), C.int(item.id_len))
		}
		if item.text != nil {
			hit.Text = C.GoString(item.text)
		}
		if item.metadata_json != nil {
			hit.Metadata = C.GoString(item.metadata_json)
		}
		hits = append(hits, hit)
	}
	return hits, nil
}