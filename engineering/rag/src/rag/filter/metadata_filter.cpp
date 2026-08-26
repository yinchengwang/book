/**
 * @file metadata_filter.cpp
 * @brief 元数据过滤器实现
 */

#include "rag/metadata_filter.h"
#include "rag/logger.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <regex>
#include <sstream>

namespace rag {

// ========== MetadataFilter ==========

std::string MetadataFilter::to_sql_where(const std::string& table_alias) const {
    std::vector<std::string> conditions;
    std::string alias = table_alias.empty() ? "" : table_alias + ".";

    // 文件类型
    if (!file_types.empty()) {
        std::string types_str;
        for (const auto& type : file_types) {
            if (!types_str.empty()) types_str += ", ";
            types_str += "'" + type + "'";
        }
        conditions.push_back(alias + "file_type IN (" + types_str + ")");
    }

    // 时间范围
    if (!date_from.empty()) {
        conditions.push_back(alias + "created_at >= '" + date_from + "'");
    }
    if (!date_to.empty()) {
        conditions.push_back(alias + "created_at <= '" + date_to + "'");
    }

    // 作者
    if (!authors.empty()) {
        std::string authors_str;
        for (const auto& author : authors) {
            if (!authors_str.empty()) authors_str += ", ";
            authors_str += "'" + author + "'";
        }
        conditions.push_back(alias + "author IN (" + authors_str + ")");
    }

    // 标签
    if (!tags.empty()) {
        std::string tags_str;
        for (const auto& tag : tags) {
            if (!tags_str.empty()) tags_str += ", ";
            tags_str += "'" + tag + "'";
        }
        if (tags_match_all) {
            conditions.push_back(alias + "tags @> ARRAY[" + tags_str + "]");
        } else {
            conditions.push_back(alias + "tags && ARRAY[" + tags_str + "]");
        }
    }

    // 语言
    if (!languages.empty()) {
        std::string langs_str;
        for (const auto& lang : languages) {
            if (!langs_str.empty()) langs_str += ", ";
            langs_str += "'" + lang + "'";
        }
        conditions.push_back(alias + "language IN (" + langs_str + ")");
    }

    // 来源
    if (!sources.empty()) {
        std::string sources_str;
        for (const auto& source : sources) {
            if (!sources_str.empty()) sources_str += ", ";
            sources_str += "'" + source + "'";
        }
        conditions.push_back(alias + "source IN (" + sources_str + ")");
    }

    // 自定义字段
    for (const auto& [field, value] : custom_filters) {
        if (std::holds_alternative<std::string>(value)) {
            conditions.push_back(alias + field + " = '" + std::get<std::string>(value) + "'");
        } else if (std::holds_alternative<std::vector<std::string>>(value)) {
            const auto& values = std::get<std::vector<std::string>>(value);
            std::string values_str;
            for (const auto& v : values) {
                if (!values_str.empty()) values_str += ", ";
                values_str += "'" + v + "'";
            }
            conditions.push_back(alias + field + " IN (" + values_str + ")");
        }
    }

    // 组合条件
    std::string result;
    for (size_t i = 0; i < conditions.size(); ++i) {
        if (i > 0) result += " AND ";
        result += conditions[i];
    }

    return result;
}

MetadataFilter MetadataFilter::from_json(const std::string& json_str) {
    MetadataFilter filter;
    // 简单的 JSON 解析
    // 实际应该使用 JSON 库

    // 文件类型
    std::regex file_types_regex(R"("file_types"\s*:\s*\[([^\]]*)\])");
    std::smatch match;
    if (std::regex_search(json_str, match, file_types_regex)) {
        std::string types_str = match[1].str();
        std::regex type_regex(R"("([^"]+)")");
        std::sregex_iterator it(types_str.begin(), types_str.end(), type_regex);
        while (it != std::sregex_iterator()) {
            filter.file_types.insert((*it)[1].str());
            ++it;
        }
    }

    // 日期范围
    std::regex from_regex(R"("date_from"\s*:\s*"([^"]*)")");
    if (std::regex_search(json_str, match, from_regex)) {
        filter.date_from = match[1].str();
    }

    std::regex to_regex(R"("date_to"\s*:\s*"([^"]*)")");
    if (std::regex_search(json_str, match, to_regex)) {
        filter.date_to = match[1].str();
    }

    return filter;
}

// ========== AtomicFilter ==========

std::string AtomicFilter::to_sql() const {
    std::string field_sql = "\"" + field + "\"";

    switch (op) {
        case Op::EQ:
            if (std::holds_alternative<std::string>(value)) {
                return field_sql + " = '" + std::get<std::string>(value) + "'";
            }
            break;

        case Op::NE:
            if (std::holds_alternative<std::string>(value)) {
                return field_sql + " != '" + std::get<std::string>(value) + "'";
            }
            break;

        case Op::GT:
            if (std::holds_alternative<double>(value)) {
                return field_sql + " > " + std::to_string(std::get<double>(value));
            }
            break;

        case Op::GE:
            if (std::holds_alternative<double>(value)) {
                return field_sql + " >= " + std::to_string(std::get<double>(value));
            }
            break;

        case Op::LT:
            if (std::holds_alternative<double>(value)) {
                return field_sql + " < " + std::to_string(std::get<double>(value));
            }
            break;

        case Op::LE:
            if (std::holds_alternative<double>(value)) {
                return field_sql + " <= " + std::to_string(std::get<double>(value));
            }
            break;

        case Op::IN:
            if (std::holds_alternative<std::vector<std::string>>(value)) {
                const auto& values = std::get<std::vector<std::string>>(value);
                std::string values_str;
                for (const auto& v : values) {
                    if (!values_str.empty()) values_str += ", ";
                    values_str += "'" + v + "'";
                }
                return field_sql + " IN (" + values_str + ")";
            }
            break;

        case Op::NOT_IN:
            if (std::holds_alternative<std::vector<std::string>>(value)) {
                const auto& values = std::get<std::vector<std::string>>(value);
                std::string values_str;
                for (const auto& v : values) {
                    if (!values_str.empty()) values_str += ", ";
                    values_str += "'" + v + "'";
                }
                return field_sql + " NOT IN (" + values_str + ")";
            }
            break;

        case Op::LIKE:
            if (std::holds_alternative<std::string>(value)) {
                return field_sql + " LIKE '%" + std::get<std::string>(value) + "%'";
            }
            break;

        case Op::BETWEEN:
            // 需要两个值，未实现
            break;
    }

    return "";
}

// ========== CompositeFilter ==========

CompositeFilter CompositeFilter::and_(const std::vector<CompositeFilter>& sub_filters) {
    CompositeFilter result;
    result.logic = Logic::AND;
    result.children = sub_filters;
    return result;
}

CompositeFilter CompositeFilter::or_(const std::vector<CompositeFilter>& sub_filters) {
    CompositeFilter result;
    result.logic = Logic::OR;
    result.children = sub_filters;
    return result;
}

CompositeFilter CompositeFilter::not_(const CompositeFilter& sub_filter) {
    CompositeFilter result;
    result.logic = Logic::NOT;
    result.children = {sub_filter};
    return result;
}

std::string CompositeFilter::to_sql() const {
    if (children.empty() && atoms.empty()) {
        return "1=1";
    }

    std::string op_str = (logic == Logic::AND) ? " AND " :
                         (logic == Logic::OR) ? " OR " : "NOT ";

    std::string result;
    bool first = true;

    // 子表达式
    for (const auto& child : children) {
        if (!first) result += op_str;
        if (logic == Logic::NOT) {
            result += "(NOT " + child.to_sql() + ")";
        } else {
            result += "(" + child.to_sql() + ")";
        }
        first = false;
    }

    // 原子表达式
    for (const auto& atom : atoms) {
        if (!first) result += op_str;
        result += atom.to_sql();
        first = false;
    }

    return result.empty() ? "1=1" : result;
}

// ========== MetadataFilterEngine ==========

std::vector<RetrievalResult> MetadataFilterEngine::apply(
    const std::vector<RetrievalResult>& results,
    const MetadataFilter& filter) {

    if (filter.empty()) {
        return results;
    }

    std::vector<RetrievalResult> filtered;
    filtered.reserve(results.size());

    for (const auto& result : results) {
        if (match_chunk(result.chunk, filter)) {
            filtered.push_back(result);
        }
    }

    return filtered;
}

std::vector<Chunk> MetadataFilterEngine::filter_chunks(
    const std::vector<Chunk>& chunks,
    const MetadataFilter& filter) {

    if (filter.empty()) {
        return chunks;
    }

    std::vector<Chunk> filtered;
    filtered.reserve(chunks.size());

    for (const auto& chunk : chunks) {
        if (match_chunk(chunk, filter)) {
            filtered.push_back(chunk);
        }
    }

    return filtered;
}

std::vector<int> MetadataFilterEngine::apply_pushdown(
    const MetadataFilter& filter,
    std::shared_ptr<void> index_connection) {

    // 如果没有数据库连接，不下推
    if (!index_connection) {
        return {};
    }

    // 生成 SQL 并执行
    // 实际实现需要数据库连接
    std::string sql = generate_sql_where(filter, "chunks");
    if (sql.empty()) {
        return {};  // 无过滤条件
    }

    // TODO: 执行 SQL 查询返回 ID 列表
    // 这里返回空表示不下推
    return {};
}

std::string MetadataFilterEngine::generate_sql_where(
    const MetadataFilter& filter,
    const std::string& table_name) {

    return filter.to_sql_where(table_name);
}

std::vector<RetrievalResult> MetadataFilterEngine::filter_by_expression(
    const std::vector<RetrievalResult>& results,
    const CompositeFilter& expression) {

    std::vector<RetrievalResult> filtered;
    std::string sql = expression.to_sql();

    for (const auto& result : results) {
        auto metadata = extract_metadata(result.chunk);

        // 简单的 SQL 条件评估
        // 实际应该使用表达式求值器
        bool matches = evaluate_expression(metadata, expression);
        if (matches) {
            filtered.push_back(result);
        }
    }

    return filtered;
}

std::string MetadataFilterEngine::compile_to_sql(const CompositeFilter& expression) {
    return expression.to_sql();
}

int64_t MetadataFilterEngine::parse_date(const std::string& date_str) {
    if (date_str.empty()) {
        return 0;
    }

    std::tm tm = {};
    std::istringstream ss(date_str);
    ss >> std::get_time(&tm, "%Y-%m-%d");
    if (ss.fail()) {
        return 0;
    }

    return std::mktime(&tm);
}

bool MetadataFilterEngine::match_pattern(const std::string& value, const std::string& pattern) {
    // 简单实现: 检查是否包含
    return value.find(pattern) != std::string::npos;
}

std::unordered_map<std::string, std::string> MetadataFilterEngine::extract_metadata(
    const Chunk& chunk) {

    std::unordered_map<std::string, std::string> metadata;

    // 从 chunk 的 metadata 字段提取
    for (const auto& [key, value] : chunk.metadata) {
        metadata[key] = value;
    }

    // 补充常用字段
    if (!chunk.file_path.empty()) {
        // 提取文件扩展名
        size_t pos = chunk.file_path.find_last_of('.');
        if (pos != std::string::npos) {
            metadata["file_type"] = chunk.file_path.substr(pos + 1);
        }
        metadata["source"] = chunk.file_path;
    }

    return metadata;
}

bool MetadataFilterEngine::match_chunk(const Chunk& chunk, const MetadataFilter& filter) const {
    auto metadata = extract_metadata(chunk);

    // 文件类型
    if (!filter.file_types.empty()) {
        auto it = metadata.find("file_type");
        if (it == metadata.end() ||
            filter.file_types.find(it->second) == filter.file_types.end()) {
            return false;
        }
    }

    // 日期范围
    if (!filter.date_from.empty()) {
        auto it = metadata.find("created_at");
        if (it != metadata.end()) {
            int64_t chunk_date = parse_date(it->second);
            int64_t from_date = parse_date(filter.date_from);
            if (chunk_date < from_date) return false;
        }
    }

    if (!filter.date_to.empty()) {
        auto it = metadata.find("created_at");
        if (it != metadata.end()) {
            int64_t chunk_date = parse_date(it->second);
            int64_t to_date = parse_date(filter.date_to);
            if (chunk_date > to_date) return false;
        }
    }

    // 作者
    if (!filter.authors.empty()) {
        auto it = metadata.find("author");
        if (it == metadata.end() ||
            filter.authors.find(it->second) == filter.authors.end()) {
            return false;
        }
    }

    // 标签 (OR 匹配)
    if (!filter.tags.empty()) {
        auto it = metadata.find("tags");
        if (it == metadata.end()) {
            return false;
        }
        // 解析标签列表
        std::vector<std::string> chunk_tags;
        std::istringstream ss(it->second);
        std::string tag;
        while (std::getline(ss, tag, ',')) {
            chunk_tags.push_back(tag);
        }

        if (filter.tags_match_all) {
            // AND: 所有标签都必须匹配
            for (const auto& required_tag : filter.tags) {
                if (std::find(chunk_tags.begin(), chunk_tags.end(), required_tag) == chunk_tags.end()) {
                    return false;
                }
            }
        } else {
            // OR: 任意一个标签匹配即可
            bool found = false;
            for (const auto& chunk_tag : chunk_tags) {
                if (filter.tags.find(chunk_tag) != filter.tags.end()) {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
    }

    // 语言
    if (!filter.languages.empty()) {
        auto it = metadata.find("language");
        if (it == metadata.end() ||
            filter.languages.find(it->second) == filter.languages.end()) {
            return false;
        }
    }

    // 来源
    if (!filter.sources.empty()) {
        auto it = metadata.find("source");
        if (it == metadata.end()) {
            return false;
        }
        bool found = false;
        for (const auto& source : filter.sources) {
            if (it->second.find(source) != std::string::npos) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    // 自定义字段
    for (const auto& [field, value] : filter.custom_filters) {
        auto it = metadata.find(field);
        if (it == metadata.end()) {
            return false;
        }

        if (std::holds_alternative<std::string>(value)) {
            if (it->second != std::get<std::string>(value)) {
                return false;
            }
        } else if (std::holds_alternative<std::vector<std::string>>(value)) {
            const auto& values = std::get<std::vector<std::string>>(value);
            if (std::find(values.begin(), values.end(), it->second) == values.end()) {
                return false;
            }
        }
    }

    return true;
}

bool MetadataFilterEngine::match_atomic(const std::string& value, const AtomicFilter& atom) const {
    switch (atom.op) {
        case AtomicFilter::Op::EQ:
            if (std::holds_alternative<std::string>(atom.value)) {
                return value == std::get<std::string>(atom.value);
            }
            break;

        case AtomicFilter::Op::NE:
            if (std::holds_alternative<std::string>(atom.value)) {
                return value != std::get<std::string>(atom.value);
            }
            break;

        case AtomicFilter::Op::LIKE:
            if (std::holds_alternative<std::string>(atom.value)) {
                return match_pattern(value, std::get<std::string>(atom.value));
            }
            break;

        default:
            break;
    }
    return false;
}

// ========== FilterBuilder ==========

CompositeFilter FilterBuilder::to_expression() const {
    std::vector<CompositeFilter> sub_filters;

    if (!filter_->file_types.empty()) {
        AtomicFilter atom;
        atom.field = "file_type";
        atom.op = AtomicFilter::Op::IN;
        atom.value = std::vector<std::string>(filter_->file_types.begin(), filter_->file_types.end());
        // ...
    }

    return CompositeFilter::and_(sub_filters);
}

// ========== Factory ==========

std::shared_ptr<MetadataFilterEngine> create_filter_engine() {
    return std::make_shared<MetadataFilterEngine>();
}

std::shared_ptr<MetadataFilter> create_empty_filter() {
    return std::make_shared<MetadataFilter>();
}

std::shared_ptr<MetadataFilter> parse_filter_from_json(const std::string& json_str) {
    auto filter = std::make_shared<MetadataFilter>();
    *filter = MetadataFilter::from_json(json_str);
    return filter;
}

}  // namespace rag
