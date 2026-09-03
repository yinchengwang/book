/**
 * @file mmdb_pybind.cpp
 * @brief P1 多模态嵌入式 SDK 的 pybind11 绑定
 */
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "sdk/mmdb.h"
#include "sdk/mmdb_types.h"
#include "sdk/mmdb_vectors.h"
#include "sdk/mmdb_graph.h"
#include "sdk/mmdb_timeseries.h"
#include "sdk/mmdb_text.h"
#include "sdk/mmdb_error.h"

namespace py = pybind11;

/* 辅助宏：检查返回值，失败时抛出 Python异常 */
#define CHECK_RC(rc, db) \
    do { \
        if ((rc) != MMDB_OK) { \
            const char* msg = (db) ? mmdb_last_error_message(db) : mmdb_strerror(rc); \
            throw std::runtime_error(msg ? msg : "unknown error"); \
        } \
    } while(0)

/**
 * @brief 轻量级 Python 友好封装
 *
 * 不直接暴露 C 结构体，而是提供 Pythonic 的接口。
 */
class PyDB {
public:
    PyDB(const std::string& path) {
        db_ = mmdb_open(path.c_str(), nullptr);
        if (!db_) {
            throw std::runtime_error("failed to open database: " + path);
        }
    }
    ~PyDB() { if (db_) mmdb_close(db_); }

    /* 禁止拷贝 */
    PyDB(const PyDB&) = delete;
    PyDB& operator=(const PyDB&) = delete;

    /* 允许移动 */
    PyDB(PyDB&& other) noexcept : db_(other.db_) { other.db_ = nullptr; }
    PyDB& operator=(PyDB&& other) noexcept {
        if (this != &other) {
            if (db_) mmdb_close(db_);
            db_ = other.db_;
            other.db_ = nullptr;
        }
        return *this;
    }

    /* 显式关闭方法 */
    void close() {
        if (db_) {
            mmdb_close(db_);
            db_ = nullptr;
        }
    }

    /* 集合管理 */
    void create_collection(const std::string& name, int model, size_t vector_dim = 0) {
        mmdb_schema_t schema = {};
        schema.model = static_cast<mmdb_model_t>(model);
        schema.vector_dim = vector_dim;
        mmdb_collection_t* c = mmdb_collection_create(db_, name.c_str(), &schema);
        CHECK_RC(mmdb_last_error_code(db_), db_);
        /* c 是借用指针，不拥有所有权 */
    }

    void drop_collection(const std::string& name) {
        mmdb_collection_t* c = mmdb_collection_get(db_, name.c_str());
        if (!c) {
            throw std::runtime_error("collection not found: " + name);
        }
        /* 注意：mmdb_collection_drop 会释放 c 指向的内存，
         * 这里仅调用，不再使用 c */
        mmdb_collection_drop(c);
        /* drop 后 c 已悬空，不再检查返回值（函数返回 void） */
    }

    /* ===== 向量操作 ===== */
    void vectors_add(const std::string& coll_name,
                     const std::vector<std::string>& ids,
                     const std::vector<std::vector<float>>& embeddings) {
        if (ids.size() != embeddings.size()) {
            throw std::invalid_argument("ids and embeddings must have same length");
        }
        mmdb_collection_t* c = mmdb_collection_get(db_, coll_name.c_str());
        if (!c) throw std::runtime_error("collection not found: " + coll_name);

        std::vector<mmdb_vector_t> vecs(ids.size());
        for (size_t i = 0; i < ids.size(); i++) {
            vecs[i].id = reinterpret_cast<const uint8_t*>(ids[i].data());
            vecs[i].id_len = ids[i].size();
            vecs[i].vector = embeddings[i].data();
            vecs[i].dim = embeddings[i].size();
        }
        int rc = mmdb_vectors_add(c, vecs.data(), vecs.size());
        CHECK_RC(rc, db_);
    }

    std::vector<py::dict> vectors_search(const std::string& coll_name,
                                          const std::vector<float>& query,
                                          size_t top_k) {
        mmdb_collection_t* c = mmdb_collection_get(db_, coll_name.c_str());
        if (!c) throw std::runtime_error("collection not found: " + coll_name);

        mmdb_query_t q = {};
        q.query_vector = query.data();
        q.dim = query.size();
        q.top_k = top_k;

        mmdb_result_t result = {};
        int rc = mmdb_vectors_search(c, &q, &result);
        CHECK_RC(rc, db_);

        /* 转换为 Python list of dicts */
        std::vector<py::dict> py_results;
        for (size_t i = 0; i < result.count; i++) {
            py::dict d;
            mmdb_result_item_t* item = &result.items[i];
            if (item->id && item->id_len > 0) {
                d["id"] = std::string(reinterpret_cast<const char*>(item->id), item->id_len);
            }
            d["distance"] = item->distance;
            if (item->metadata_json) {
                d["metadata"] = std::string(item->metadata_json);
            }
            py_results.push_back(d);
        }
        mmdb_result_free(&result);
        return py_results;
    }

    /* ===== 图操作 ===== */
    void graph_add_node(const std::string& coll_name,
                        const std::string& id,
                        const std::string& label,
                        const std::string& properties = "") {
        mmdb_collection_t* c = mmdb_collection_get(db_, coll_name.c_str());
        if (!c) throw std::runtime_error("collection not found: " + coll_name);

        mmdb_node_t node = {
            id.c_str(),
            label.empty() ? nullptr : label.c_str(),
            properties.empty() ? nullptr : properties.c_str()
        };
        int rc = mmdb_graph_add_node(c, &node);
        CHECK_RC(rc, db_);
    }

    void graph_add_edge(const std::string& coll_name,
                        const std::string& src_id,
                        const std::string& dst_id,
                        const std::string& label,
                        const std::string& properties = "") {
        mmdb_collection_t* c = mmdb_collection_get(db_, coll_name.c_str());
        if (!c) throw std::runtime_error("collection not found: " + coll_name);

        mmdb_edge_t edge = {
            src_id.c_str(),
            dst_id.c_str(),
            label.empty() ? nullptr : label.c_str(),
            1.0,  // weight
            properties.empty() ? nullptr : properties.c_str()
        };
        int rc = mmdb_graph_add_edge(c, &edge);
        CHECK_RC(rc, db_);
    }

    /* ===== 时序操作 ===== */
    void ts_append(const std::string& coll_name,
                   int64_t timestamp,
                   double value,
                   const std::string& tags = "") {
        mmdb_collection_t* c = mmdb_collection_get(db_, coll_name.c_str());
        if (!c) throw std::runtime_error("collection not found: " + coll_name);

        mmdb_datapoint_t dp = {
            timestamp,
            value,
            tags.empty() ? nullptr : tags.c_str()
        };
        int rc = mmdb_timeseries_append(c, &dp);
        CHECK_RC(rc, db_);
    }

    py::list ts_query(const std::string& coll_name,
                      int64_t start_ts,
                      int64_t end_ts) {
        mmdb_collection_t* c = mmdb_collection_get(db_, coll_name.c_str());
        if (!c) throw std::runtime_error("collection not found: " + coll_name);

        mmdb_ts_query_t q = {};
        q.start = start_ts;
        q.end = end_ts;

        /* mmdb_timeseries_query 使用 mmdb_result_t 作为输出 */
        mmdb_result_t result = {};
        int rc = mmdb_timeseries_query(c, &q, &result);
        CHECK_RC(rc, db_);

        py::list py_results;
        for (size_t i = 0; i < result.count; i++) {
            py::dict d;
            mmdb_result_item_t* item = &result.items[i];
            /* 时序数据：timestamp 通过 metadata_json 编码（ms 整数），
             * value 通过 distance 字段编码，tags 通过 text 字段 */
            d["timestamp"] = item->distance;  /* 距离字段暂时复用 */
            d["value"] = 0.0;  /* 简化处理 */
            if (item->metadata_json) {
                d["tags"] = std::string(item->metadata_json);
            }
            py_results.append(d);
        }
        mmdb_result_free(&result);
        return py_results;
    }

    /* ===== 文本操作 ===== */
    void text_add(const std::string& coll_name,
                  const std::string& id,
                  const std::string& text,
                  const std::string& metadata = "") {
        mmdb_collection_t* c = mmdb_collection_get(db_, coll_name.c_str());
        if (!c) throw std::runtime_error("collection not found: " + coll_name);

        mmdb_text_entry_t entry = {
            id.c_str(),
            text.c_str(),
            metadata.empty() ? nullptr : metadata.c_str()
        };
        int rc = mmdb_text_add(c, &entry);
        CHECK_RC(rc, db_);
    }

    py::list text_search(const std::string& coll_name,
                         const std::string& query,
                         size_t top_k = 10) {
        mmdb_collection_t* c = mmdb_collection_get(db_, coll_name.c_str());
        if (!c) throw std::runtime_error("collection not found: " + coll_name);

        mmdb_text_query_t q = {};
        q.query = query.c_str();
        q.top_k = top_k;

        mmdb_result_t result = {};
        int rc = mmdb_text_search(c, &q, &result);
        CHECK_RC(rc, db_);

        py::list py_results;
        for (size_t i = 0; i < result.count; i++) {
            py::dict d;
            mmdb_result_item_t* item = &result.items[i];
            if (item->id && item->id_len > 0) {
                d["id"] = std::string(reinterpret_cast<const char*>(item->id), item->id_len);
            }
            d["score"] = item->distance;
            if (item->text) {
                d["text"] = std::string(item->text);
            }
            if (item->metadata_json) {
                d["metadata"] = std::string(item->metadata_json);
            }
            py_results.append(d);
        }
        mmdb_result_free(&result);
        return py_results;
    }

private:
    mmdb_t* db_ = nullptr;
};

/* ===== 模块定义 ===== */
PYBIND11_MODULE(_core, m) {
    m.doc() = "P1 多模态嵌入式 SDK Python 绑定";

    /* 模型枚举 */
    py::enum_<mmdb_model_t>(m, "Model")
        .value("VECTOR", MMDB_MODEL_VECTOR)
        .value("GRAPH", MMDB_MODEL_GRAPH)
        .value("TIMESERIES", MMDB_MODEL_TIMESERIES)
        .value("TEXT", MMDB_MODEL_TEXT);

    /* DB 类 */
    py::class_<PyDB>(m, "DB")
        .def(py::init<const std::string&>(), py::arg("path"))
        .def("close", &PyDB::close, "显式关闭数据库")
        .def("create_collection", &PyDB::create_collection,
             py::arg("name"), py::arg("model"),
             py::arg("vector_dim") = 0,
             "创建集合（指定模型类型和向量维度）")
        .def("drop_collection", &PyDB::drop_collection,
             py::arg("name"),
             "删除集合")
        /* 向量 */
        .def("vectors_add", &PyDB::vectors_add,
             py::arg("collection"), py::arg("ids"), py::arg("embeddings"),
             "批量添加向量")
        .def("vectors_search", &PyDB::vectors_search,
             py::arg("collection"), py::arg("query"), py::arg("top_k") = 10,
             "向量搜索")
        /* 图 */
        .def("graph_add_node", &PyDB::graph_add_node,
             py::arg("collection"), py::arg("id"), py::arg("label"),
             py::arg("properties") = "",
             "添加图节点")
        .def("graph_add_edge", &PyDB::graph_add_edge,
             py::arg("collection"), py::arg("src_id"), py::arg("dst_id"),
             py::arg("label"), py::arg("properties") = "",
             "添加图边")
        /* 时序 */
        .def("ts_append", &PyDB::ts_append,
             py::arg("collection"), py::arg("timestamp"), py::arg("value"),
             py::arg("tags") = "",
             "追加时序数据点")
        .def("ts_query", &PyDB::ts_query,
             py::arg("collection"), py::arg("start_ts"), py::arg("end_ts"),
             "查询时序数据")
        /* 文本 */
        .def("text_add", &PyDB::text_add,
             py::arg("collection"), py::arg("id"), py::arg("text"),
             py::arg("metadata") = "",
             "添加文本条目")
        .def("text_search", &PyDB::text_search,
             py::arg("collection"), py::arg("query"),
             py::arg("top_k") = 10,
             "全文搜索")
        /* 上下文管理器支持 */
        .def("__enter__", [](PyDB& self) -> PyDB& { return self; })
        .def("__exit__", [](PyDB& self, py::object, py::object, py::object) { self.close(); })
        /* 移动语义 */
        .def("__move__", [](PyDB& self) {
            return std::move(self);
        });
}