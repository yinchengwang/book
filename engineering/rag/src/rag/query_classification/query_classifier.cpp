/**
 * @file query_classifier.cpp
 * @brief 查询分类器实现
 */

#include "rag/query_classifier.h"
#include "rag/logger.h"
#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace rag {

// ========== BaseQueryClassifier ==========

BaseQueryClassifier::BaseQueryClassifier(const QueryClassifierConfig& config)
    : config_(config) {}

QueryType BaseQueryClassifier::classify(const std::string& query) {
    auto result = classify_with_confidence(query);
    return result.first;
}

std::pair<QueryType, float> BaseQueryClassifier::classify_with_confidence(const std::string& query) {
    auto result = do_classify(query);

    // 更新统计
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_classifications++;
        stats_.type_counts[result.type]++;
    }

    return {result.type, result.confidence};
}

std::vector<std::string> BaseQueryClassifier::extract_keywords(const std::string& query) {
    return do_extract_keywords(query);
}

BaseQueryClassifier::ClassifierStats BaseQueryClassifier::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

// ========== RuleBasedQueryClassifier ==========

RuleBasedQueryClassifier::RuleBasedQueryClassifier(const QueryClassifierConfig& config)
    : BaseQueryClassifier(config) {
    init_rules();
}

void RuleBasedQueryClassifier::init_rules() {
    // FACTUAL: 事实查询
    patterns_[QueryType::FACTUAL] = {
        R"(什么是|who is|谁.*)",
        R"(是什么|what is|what are)",
        R"(多少|how many|how much)",
        R"(哪个|which)",
        R"(何时|when did|when will)",
        R"(何地|where is|where did)"
    };

    // COMPARATIVE: 比较查询
    patterns_[QueryType::COMPARATIVE] = {
        R"(.*比.*|.*vs.*|.*versus.*)",
        R"(区别|difference between)",
        R"(比较|compare.*with|compare.*to)",
        R"(更好|better|worse)",
        R"(优缺点|pros and cons|advantages.*disadvantages)"
    };

    // ANALYTICAL: 分析查询
    patterns_[QueryType::ANALYTICAL] = {
        R"(为什么|why.*|reason)",
        R"(如何|how.*|method|approach)",
        R"(分析|analyze|analysis)",
        R"(解释|explain|explanation)",
        R"(原理|principle|mechanism)"
    };

    // SUMMARY: 总结查询
    patterns_[QueryType::SUMMARY] = {
        R"(总结|summarize|summary)",
        R"(概述|overview|outline)",
        R"(要点|key points|main points)",
        R"(介绍|introduce|introduction)",
        R"(讲解|explain.*concept)"
    };

    // MULTI_HOP: 多跳查询
    patterns_[QueryType::MULTI_HOP] = {
        R"(.*和.*的关系|relationship between.*and)",
        R"(.*的.*是什么|what is.*'s.*)",
        R"(.*属于.*|belong to)",
        R"(张三.*李四|.*and.*)"  // 人名模式
    };

    // CHAT: 闲聊
    patterns_[QueryType::CHAT] = {
        R"(你好|hello|hi|hey)",
        R"(谢谢|thank you|thanks)",
        R"(再见|goodbye|bye)",
        R"(.*吗$|.*吗？)",  // 简单判断
        R"(最近如何|how are you)"
    };

    // 停用词
    stopwords_ = {
        "的", "了", "是", "在", "和", "有", "我", "你", "他", "她", "它",
        "the", "a", "an", "is", "are", "was", "were", "to", "of", "in",
        "for", "on", "with", "at", "by", "from", "as", "or", "and"
    };
}

ClassificationResult RuleBasedQueryClassifier::do_classify(const std::string& query) {
    ClassificationResult result;

    // 1. 模式匹配
    auto pattern_result = match_patterns(query);
    if (pattern_result.confidence > 0.7f) {
        return pattern_result;
    }

    // 2. 关键词分析
    auto keywords = do_extract_keywords(query);
    auto keyword_result = analyze_keywords(query, keywords);
    keyword_result.confidence = std::max(pattern_result.confidence, keyword_result.confidence);

    return keyword_result;
}

ClassificationResult RuleBasedQueryClassifier::match_patterns(const std::string& query) {
    ClassificationResult result;

    float best_score = 0.0f;
    QueryType best_type = QueryType::FACTUAL;

    for (const auto& [type, patterns] : patterns_) {
        for (const auto& pattern_str : patterns) {
            try {
                std::regex pattern(pattern_str, std::regex_constants::icase);
                if (std::regex_search(query, pattern)) {
                    float score = 0.8f;
                    if (score > best_score) {
                        best_score = score;
                        best_type = type;
                    }
                    break;
                }
            } catch (const std::regex_error&) {
                // 忽略无效正则
            }
        }
    }

    if (best_score > 0.0f) {
        result.type = best_type;
        result.confidence = best_score;
        result.type_scores[query_type_to_string(best_type)] = best_score;
    }

    return result;
}

ClassificationResult RuleBasedQueryClassifier::analyze_keywords(const std::string& query,
                                                               const std::vector<std::string>& keywords) {
    ClassificationResult result;

    // 关键词权重映射
    std::unordered_map<QueryType, float> type_scores;
    float total_score = 0.0f;

    for (const auto& keyword : keywords) {
        // FACTUAL 关键词
        if (keyword == "什么" || keyword == "谁" || keyword == "哪个" ||
            keyword == "what" || keyword == "who" || keyword == "which") {
            type_scores[QueryType::FACTUAL] += 1.0f;
        }
        // COMPARATIVE 关键词
        else if (keyword == "比" || keyword == "比较" || keyword == "difference" ||
                 keyword == "compare" || keyword == "vs") {
            type_scores[QueryType::COMPARATIVE] += 1.0f;
        }
        // ANALYTICAL 关键词
        else if (keyword == "为什么" || keyword == "如何" || keyword == "分析" ||
                 keyword == "why" || keyword == "how" || keyword == "analyze") {
            type_scores[QueryType::ANALYTICAL] += 1.0f;
        }
        // SUMMARY 关键词
        else if (keyword == "总结" || keyword == "概述" || keyword == "要点" ||
                 keyword == "summary" || keyword == "overview") {
            type_scores[QueryType::SUMMARY] += 1.0f;
        }
        // CHAT 关键词
        else if (keyword == "你好" || keyword == "谢谢" || keyword == "再见" ||
                 keyword == "hello" || keyword == "thanks" || keyword == "bye") {
            type_scores[QueryType::CHAT] += 1.0f;
        }

        total_score += 1.0f;
    }

    // 找到最高分类型
    float best_score = 0.0f;
    QueryType best_type = QueryType::FACTUAL;

    for (const auto& [type, score] : type_scores) {
        if (score > best_score) {
            best_score = score;
            best_type = type;
        }
    }

    if (total_score > 0.0f) {
        result.type = best_type;
        result.confidence = best_score / total_score;
        result.keywords = keywords;
    }

    return result;
}

std::vector<std::string> RuleBasedQueryClassifier::do_extract_keywords(const std::string& query) {
    std::vector<std::string> keywords;

    // 简单分词：按空格和标点分割
    std::string cleaned;
    for (char c : query) {
        if (std::isalnum(c) || std::isspace(c)) {
            cleaned += c;
        } else {
            cleaned += ' ';
        }
    }

    std::istringstream iss(cleaned);
    std::string word;
    while (iss >> word) {
        // 转小写
        std::transform(word.begin(), word.end(), word.begin(),
                      [](unsigned char c) { return std::tolower(c); });

        // 过滤停用词
        if (stopwords_.find(word) == stopwords_.end() && word.length() > 1) {
            keywords.push_back(word);
        }
    }

    return keywords;
}

// ========== KeywordQueryClassifier ==========

KeywordQueryClassifier::KeywordQueryClassifier(const QueryClassifierConfig& config)
    : BaseQueryClassifier(config) {

    // 默认关键词
    add_keyword(QueryType::FACTUAL, "什么", 1.0f);
    add_keyword(QueryType::FACTUAL, "谁", 1.0f);
    add_keyword(QueryType::FACTUAL, "哪个", 1.0f);
    add_keyword(QueryType::FACTUAL, "多少", 1.0f);

    add_keyword(QueryType::COMPARATIVE, "比", 1.0f);
    add_keyword(QueryType::COMPARATIVE, "比较", 1.5f);
    add_keyword(QueryType::COMPARATIVE, "区别", 1.5f);
    add_keyword(QueryType::COMPARATIVE, "vs", 2.0f);

    add_keyword(QueryType::ANALYTICAL, "为什么", 1.5f);
    add_keyword(QueryType::ANALYTICAL, "如何", 1.0f);
    add_keyword(QueryType::ANALYTICAL, "分析", 1.5f);

    add_keyword(QueryType::SUMMARY, "总结", 2.0f);
    add_keyword(QueryType::SUMMARY, "概述", 1.5f);
    add_keyword(QueryType::SUMMARY, "要点", 1.5f);

    add_keyword(QueryType::CHAT, "你好", 2.0f);
    add_keyword(QueryType::CHAT, "谢谢", 2.0f);

    add_keyword(QueryType::MULTI_HOP, "和", 0.5f);
    add_keyword(QueryType::MULTI_HOP, "关系", 1.5f);

    // 停用词
    stopwords_ = {"的", "了", "是", "在", "和"};
}

void KeywordQueryClassifier::add_keyword(QueryType type, const std::string& keyword, float weight) {
    keyword_weights_[keyword].push_back({type, weight});
}

ClassificationResult KeywordQueryClassifier::do_classify(const std::string& query) {
    ClassificationResult result;

    auto keywords = do_extract_keywords(query);
    result.keywords = keywords;

    // 统计各类型得分
    std::unordered_map<QueryType, float> type_scores;

    for (const auto& keyword : keywords) {
        auto it = keyword_weights_.find(keyword);
        if (it != keyword_weights_.end()) {
            for (const auto& [type, weight] : it->second) {
                type_scores[type] += weight;
            }
        }
    }

    // 找到最高分类型
    float best_score = 0.0f;
    float total_score = 0.0f;

    for (const auto& [type, score] : type_scores) {
        total_score += score;
        if (score > best_score) {
            best_score = score;
            result.type = type;
        }
        result.type_scores[query_type_to_string(type)] = score;
    }

    if (total_score > 0.0f) {
        result.confidence = best_score / total_score;
    }

    return result;
}

std::vector<std::string> KeywordQueryClassifier::do_extract_keywords(const std::string& query) {
    std::vector<std::string> keywords;

    // 简单分词
    std::string cleaned;
    for (char c : query) {
        if (std::isalnum(c) || std::isspace(c)) {
            cleaned += c;
        } else {
            cleaned += ' ';
        }
    }

    std::istringstream iss(cleaned);
    std::string word;
    while (iss >> word) {
        std::transform(word.begin(), word.end(), word.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        if (stopwords_.find(word) == stopwords_.end() && word.length() > 1) {
            keywords.push_back(word);
        }
    }

    return keywords;
}

// ========== LLMQueryClassifier ==========

LLMQueryClassifier::LLMQueryClassifier(
    const QueryClassifierConfig& config,
    std::shared_ptr<LLMService> llm_service)
    : BaseQueryClassifier(config), llm_service_(llm_service) {}

bool LLMQueryClassifier::is_llm_available() const {
    return llm_service_ != nullptr;
}

ClassificationResult LLMQueryClassifier::do_classify(const std::string& query) {
    if (!is_llm_available()) {
        // Fallback 到规则
        RuleBasedQueryClassifier fallback;
        return fallback.do_classify(query);
    }

    try {
        auto prompt = build_classification_prompt(query);
        auto response = llm_service_->generate(prompt);

        auto type = parse_llm_response(response);

        ClassificationResult result;
        result.type = type;
        result.confidence = 0.8f;  // LLM 分类置信度
        result.type_scores[query_type_to_string(type)] = 0.8f;

        return result;
    } catch (const std::exception& e) {
        RAG_WARN("LLM classification failed: " + std::string(e.what()));
        RuleBasedQueryClassifier fallback;
        return fallback.do_classify(query);
    }
}

ClassificationResult LLMQueryClassifier::do_extract_keywords(const std::string& query) {
    // 使用关键词提取
    KeywordQueryClassifier kw_classifier;
    return kw_classifier.do_classify(query);
}

std::string LLMQueryClassifier::build_classification_prompt(const std::string& query) {
    return R"(
请分析以下查询的类型，只能选择一个最合适的类型。

查询: )" + query + R"(

类型说明:
- FACTUAL: 事实型问题，需要精确检索（如"什么是RAG"、"谁发明了XXX"）
- ANALYTICAL: 分析型问题，需要综合多源分析原因（如"为什么XXX"、"如何分析"）
- COMPARATIVE: 比较型问题，需要对比分析（如"XXX和YYY的区别"、"比较A和B"）
- SUMMARY: 总结型问题，需要摘要生成（如"总结XXX"、"概述YYY"）
- CHAT: 闲聊型，可能不需要检索（如"你好"、"谢谢"）
- MULTI_HOP: 多跳问题，需要知识图谱推理（如"XXX和YYY的关系"）

请只输出类型名称，不要其他内容。
)";
}

std::string LLMQueryClassifier::build_keyword_prompt(const std::string& query) {
    return R"(
请提取以下查询的关键词，只返回关键词列表，用逗号分隔。

查询: )" + query + R"(
)";
}

QueryType LLMQueryClassifier::parse_llm_response(const std::string& response) {
    std::string lower = response;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                  [](unsigned char c) { return std::tolower(c); });

    if (lower.find("factual") != std::string::npos) return QueryType::FACTUAL;
    if (lower.find("analytical") != std::string::npos) return QueryType::ANALYTICAL;
    if (lower.find("comparative") != std::string::npos) return QueryType::COMPARATIVE;
    if (lower.find("summary") != std::string::npos) return QueryType::SUMMARY;
    if (lower.find("chat") != std::string::npos) return QueryType::CHAT;
    if (lower.find("multi_hop") != std::string::npos) return QueryType::MULTI_HOP;

    return QueryType::FACTUAL;  // 默认
}

std::vector<std::string> LLMQueryClassifier::parse_keywords_response(const std::string& response) {
    std::vector<std::string> keywords;
    std::istringstream iss(response);
    std::string word;

    while (std::getline(iss, word, ',')) {
        // 清理
        word.erase(word.begin(),
                  std::find_if(word.begin(), word.end(),
                              [](unsigned char c) { return !std::isspace(c); }));
        if (!word.empty()) {
            keywords.push_back(word);
        }
    }

    return keywords;
}

// ========== EnsembleQueryClassifier ==========

EnsembleQueryClassifier::EnsembleQueryClassifier(
    const QueryClassifierConfig& config,
    std::shared_ptr<LLMService> llm_service)
    : BaseQueryClassifier(config), llm_service_(llm_service) {

    // 默认添加规则分类器
    add_classifier("rule",
                   std::make_shared<RuleBasedQueryClassifier>(config),
                   0.4f);

    add_classifier("keyword",
                   std::make_shared<KeywordQueryClassifier>(config),
                   0.3f);
}

void EnsembleQueryClassifier::add_classifier(const std::string& name,
                                           std::shared_ptr<BaseQueryClassifier> classifier,
                                           float weight) {
    classifiers_.push_back({classifier, weight});
}

ClassificationResult EnsembleQueryClassifier::do_classify(const std::string& query) {
    ClassificationResult result;
    std::unordered_map<QueryType, float> aggregated_scores;

    float total_weight = 0.0f;

    for (const auto& [classifier, weight] : classifiers_) {
        auto classifier_result = classifier->do_classify(query);
        aggregated_scores[classifier_result.type] += weight * classifier_result.confidence;
        total_weight += weight;

        // 记录各分类器得分
        for (const auto& [type_str, score] : classifier_result.type_scores) {
            result.type_scores[type_str] += weight * score;
        }
    }

    // 归一化
    if (total_weight > 0.0f) {
        for (auto& [type, score] : aggregated_scores) {
            score /= total_weight;
        }
    }

    // 找到最高分
    float best_score = 0.0f;
    for (const auto& [type, score] : aggregated_scores) {
        if (score > best_score) {
            best_score = score;
            result.type = type;
        }
    }

    result.confidence = best_score;
    return result;
}

std::vector<std::string> EnsembleQueryClassifier::do_extract_keywords(const std::string& query) {
    // 使用关键词分类器
    for (const auto& [classifier, weight] : classifiers_) {
        if (weight > 0.3f) {
            return classifier->extract_keywords(query);
        }
    }
    return {};
}

// ========== Factory ==========

std::shared_ptr<QueryClassifier> create_query_classifier(
    const QueryClassifierConfig& config,
    std::shared_ptr<LLMService> llm_service) {

    switch (config.strategy) {
        case QueryClassifierConfig::Strategy::RULE_ONLY:
            return std::make_shared<RuleBasedQueryClassifier>(config);

        case QueryClassifierConfig::Strategy::KEYWORD_ONLY:
            return std::make_shared<KeywordQueryClassifier>(config);

        case QueryClassifierConfig::Strategy::LLM_ASSISTED:
            return std::make_shared<LLMQueryClassifier>(config, llm_service);

        case QueryClassifierConfig::Strategy::ENSEMBLE:
            return std::make_shared<EnsembleQueryClassifier>(config, llm_service);

        default:
            return std::make_shared<RuleBasedQueryClassifier>(config);
    }
}

}  // namespace rag
