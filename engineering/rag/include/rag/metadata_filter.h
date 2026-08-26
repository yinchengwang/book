/**
 * @file metadata_filter.h
 * @brief 元数据过滤器 - 支持复杂条件过滤
 */
#pragma once

#include "rag/pipeline.h"
#include "rag/retriever.h"
#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <unordered_map>
#include <unordered_set>

namespace rag {

// ========== 元数据过滤条件 ==========

struct MetadataFilter {
    // 文件类型过滤
    std::unordered_set<std::string> file_types;

    // 时间范围
    std::string date_from;
    std::string date_to;

    // 作者过滤
    std::unordered_set<std::string> authors;

    // 标签过滤
    std::unordered_set<std::string> tags;
    bool tags_match_all = false;  // true: AND, false: OR

    // 语言过滤
    std::unordered_set<std::string> languages;

    // 来源过滤
    std::unordered_set<std::string> sources;

    // 自定义字段过滤
    std::unordered_map<std::string, std::variant<std::string, std::vector<std::string>>> custom_filters;

    // 组合逻辑
    bool empty() const {
        return file_types.empty() && date_from.empty() && date_to.empty() &&
               authors.empty() && tags.empty() && languages.empty() &&
               sources.empty() && custom_filters.empty();
    }

    // 转换为 SQL WHERE 子句
    std::string to_sql_where(const std::string& table_alias = "") const;

    // 从 JSON 解析
    static MetadataFilter from_json(const std::string& json_str);
};

// ========== 过滤表达式 ==========

/**
 * @brief 原子过滤表达式
 */
struct AtomicFilter {
    enum class Op {
        EQ,      // 等于
        NE,      // 不等于
        GT,      // 大于
        GE,      // 大于等于
        LT,      // 小于
        LE,      // 小于等于
        IN,      // 在集合中
        NOT_IN,  // 不在集合中
        LIKE,    // 模糊匹配
        BETWEEN, // 范围
    };

    std::string field;
    Op op;
    std::variant<std::string, int, double, std::vector<std::string>> value;

    std::string to_sql() const;
};

/**
 * @brief 复合过滤表达式
 */
struct CompositeFilter {
    enum class Logic {
        AND,
        OR,
        NOT
    };

    Logic logic;
    std::vector<CompositeFilter> children;
    std::vector<AtomicFilter> atoms;

    static CompositeFilter and_(const std::vector<CompositeFilter>& sub_filters);
    static CompositeFilter or_(const std::vector<CompositeFilter>& sub_filters);
    static CompositeFilter not_(const CompositeFilter& sub_filter);

    std::string to_sql() const;
};

// ========== MetadataFilterEngine ==========

/**
 * @brief 元数据过滤器引擎
 *
 * 支持:
 * 1. 内存过滤 (pushdown 后)
 * 2. 索引层预过滤 (pushdown)
 * 3. SQL WHERE 子句生成
 */
class MetadataFilterEngine {
public:
    MetadataFilterEngine() = default;
    ~MetadataFilterEngine() = default;

    // ========== 基本过滤 ==========

    /**
     * @brief 对检索结果应用过滤
     * @param results 检索结果
     * @param filter 过滤条件
     * @return 过滤后的结果
     */
    std::vector<RetrievalResult> apply(
        const std::vector<RetrievalResult>& results,
        const MetadataFilter& filter);

    /**
     * @brief 对 chunks 应用过滤
     */
    std::vector<Chunk> filter_chunks(
        const std::vector<Chunk>& chunks,
        const MetadataFilter& filter);

    // ========== Pushdown 过滤 ==========

    /**
     * @brief 预过滤 - 生成可下推到索引层的条件
     * @param filter 过滤条件
     * @return 预过滤后的候选 ID 列表 (为空表示不下推)
     */
    std::vector<int> apply_pushdown(
        const MetadataFilter& filter,
        std::shared_ptr<void> index_connection = nullptr);

    /**
     * @brief 生成 SQL WHERE 子句
     */
    std::string generate_sql_where(
        const MetadataFilter& filter,
        const std::string& table_name = "chunks");

    // ========== 表达式过滤 ==========

    /**
     * @brief 使用复合表达式过滤
     */
    std::vector<RetrievalResult> filter_by_expression(
        const std::vector<RetrievalResult>& results,
        const CompositeFilter& expression);

    /**
     * @brief 编译表达式为 SQL
     */
    std::string compile_to_sql(const CompositeFilter& expression);

    // ========== 工具方法 ==========

    /**
     * @brief 解析日期字符串
     * @param date_str ISO 格式日期 "YYYY-MM-DD"
     * @return 时间戳
     */
    static int64_t parse_date(const std::string& date_str);

    /**
     * @brief 检查字符串是否匹配模式
     */
    static bool match_pattern(const std::string& value, const std::string& pattern);

    /**
     * @brief 从 chunk 提取元数据字段
     */
    static std::unordered_map<std::string, std::string> extract_metadata(const Chunk& chunk);

private:
    bool match_chunk(const Chunk& chunk, const MetadataFilter& filter) const;
    bool match_atomic(const std::string& value, const AtomicFilter& atom) const;
};

// ========== FilterBuilder ==========

/**
 * @brief 过滤器构建器
 *
 * 使用流畅接口构建复杂的过滤条件
 */
class FilterBuilder {
public:
    FilterBuilder() : filter_(std::make_shared<MetadataFilter>()) {}

    /**
     * @brief 添加文件类型过滤
     */
    FilterBuilder& with_file_types(std::vector<std::string> types) {
        filter_->file_types.insert(types.begin(), types.end());
        return *this;
    }

    /**
     * @brief 添加时间范围过滤
     */
    FilterBuilder& with_date_range(const std::string& from, const std::string& to) {
        filter_->date_from = from;
        filter_->date_to = to;
        return *this;
    }

    /**
     * @brief 添加作者过滤
     */
    FilterBuilder& with_authors(std::vector<std::string> authors) {
        filter_->authors.insert(authors.begin(), authors.end());
        return *this;
    }

    /**
     * @brief 添加标签过滤
     */
    FilterBuilder& with_tags(std::vector<std::string> tags, bool match_all = false) {
        filter_->tags.insert(tags.begin(), tags.end());
        filter_->tags_match_all = match_all;
        return *this;
    }

    /**
     * @brief 添加语言过滤
     */
    FilterBuilder& with_languages(std::vector<std::string> languages) {
        filter_->languages.insert(languages.begin(), languages.end());
        return *this;
    }

    /**
     * @brief 添加来源过滤
     */
    FilterBuilder& with_sources(std::vector<std::string> sources) {
        filter_->sources.insert(sources.begin(), sources.end());
        return *this;
    }

    /**
     * @brief 添加自定义字段过滤
     */
    FilterBuilder& with_custom(const std::string& field, const std::string& value) {
        filter_->custom_filters[field] = value;
        return *this;
    }

    /**
     * @brief 构建过滤器
     */
    std::shared_ptr<MetadataFilter> build() {
        return filter_;
    }

    /**
     * @brief 转换为复合表达式
     */
    CompositeFilter to_expression() const;

private:
    std::shared_ptr<MetadataFilter> filter_;
};

// ========== Factory ==========

/**
 * @brief 创建过滤器引擎
 */
std::shared_ptr<MetadataFilterEngine> create_filter_engine();

/**
     * @brief 创建空过滤器
     */
    std::shared_ptr<MetadataFilter> create_empty_filter();

/**
 * @brief 解析 JSON 为过滤器
 */
std::shared_ptr<MetadataFilter> parse_filter_from_json(const std::string& json_str);

}  // namespace rag
