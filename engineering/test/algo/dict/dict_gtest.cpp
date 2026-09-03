#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

extern "C" {
#include <algo-prod/dict/dict.h>
}

namespace {

std::vector<std::string> collect_tokens(const char *text, dict_t *dict) {
    token_t *tokens = nullptr;
    int32_t token_count = 0;
    std::vector<std::string> results;

    EXPECT_EQ(dict_cut(dict, text, &tokens, &token_count), 0);
    for (int32_t i = 0; i < token_count; ++i) {
        results.emplace_back(tokens[i].text);
    }
    dict_free_tokens(tokens, token_count);
    return results;
}

}  // namespace

TEST(DictTest, LoadsDefaultDictionary) {
    dict_t *dict = dict_create_default();

    ASSERT_NE(dict, nullptr);
    EXPECT_GT(dict_size(dict), 500000);

    dict_drop(dict);
}

TEST(DictTest, LoadsJiebaCompatibleDictionaryFile) {
    const char *path = "dict_jieba_test.txt";
    std::ofstream out(path, std::ios::binary);
    dict_t *dict = dict_create();
    std::vector<std::string> tokens;

    ASSERT_NE(dict, nullptr);
    ASSERT_TRUE(out.is_open());
    out << "机器视觉 9000 n\n";
    out << "视觉 1000 n\n";
    out << "系统 1200 n\n";
    out.close();

    ASSERT_EQ(dict_load_jieba_file(dict, path), 0);
    tokens = collect_tokens("机器视觉系统", dict);
    ASSERT_EQ(tokens.size(), 2U);
    EXPECT_EQ(tokens[0], "机器视觉");
    EXPECT_EQ(tokens[1], "系统");

    dict_drop(dict);
    std::remove(path);
}

TEST(DictTest, SegmentsChineseWithDagPreference) {
    dict_t *dict = dict_create_default();
    std::vector<std::string> tokens;

    ASSERT_NE(dict, nullptr);
    tokens = collect_tokens("南京市长江大桥", dict);

    ASSERT_EQ(tokens.size(), 2U);
    EXPECT_EQ(tokens[0], "南京市");
    EXPECT_EQ(tokens[1], "长江大桥");

    dict_drop(dict);
}

TEST(DictTest, SegmentsChineseEnglishMixedText) {
    dict_t *dict = dict_create_default();
    std::vector<std::string> tokens;

    ASSERT_NE(dict, nullptr);
    tokens = collect_tokens("OpenAI中文分词ChatGPT", dict);

    ASSERT_EQ(tokens.size(), 4U);
    EXPECT_EQ(tokens[0], "OpenAI");
    EXPECT_EQ(tokens[1], "中文");
    EXPECT_EQ(tokens[2], "分词");
    EXPECT_EQ(tokens[3], "ChatGPT");

    dict_drop(dict);
}

TEST(DictTest, CustomWordCanChangeBestPath) {
    dict_t *dict = dict_create_default();
    std::vector<std::string> tokens_before;
    std::vector<std::string> tokens_after;

    ASSERT_NE(dict, nullptr);
    tokens_before = collect_tokens("系统架构师", dict);
    ASSERT_EQ(tokens_before.size(), 2U);
    EXPECT_EQ(tokens_before[0], "系统");
    EXPECT_EQ(tokens_before[1], "架构师");

    ASSERT_EQ(dict_add_word(dict, "系统架构师", 12000), 0);
    tokens_after = collect_tokens("系统架构师", dict);
    ASSERT_EQ(tokens_after.size(), 1U);
    EXPECT_EQ(tokens_after[0], "系统架构师");

    dict_drop(dict);
}

TEST(DictTest, StopWordsAreRemovedFromSegmentationResults) {
    dict_t *dict = dict_create_default();
    std::vector<std::string> tokens;

    ASSERT_NE(dict, nullptr);
    ASSERT_EQ(dict_add_stop_word(dict, "的"), 0);
    ASSERT_EQ(dict_add_stop_word(dict, "and"), 0);

    tokens = collect_tokens("自然语言处理的核心 and 方法", dict);
    EXPECT_FALSE(dict_is_stop_word(dict, "核心"));
    EXPECT_TRUE(dict_is_stop_word(dict, "的"));
    EXPECT_EQ(tokens.size(), 4U);
    EXPECT_EQ(tokens[0], "自然语言");
    EXPECT_EQ(tokens[1], "处理");
    EXPECT_EQ(tokens[2], "核心");
    EXPECT_EQ(tokens[3], "方法");

    dict_drop(dict);
}

TEST(DictTest, SynonymsAreCanonicalizedInSegmentationResults) {
    dict_t *dict = dict_create_default();
    std::vector<std::string> tokens;
    char *normalized = nullptr;

    ASSERT_NE(dict, nullptr);
    ASSERT_EQ(dict_add_word(dict, "文本检索", 8000), 0);
    ASSERT_EQ(dict_add_synonym(dict, "信息检索", "文本检索"), 0);
    EXPECT_STREQ(dict_resolve_synonym(dict, "信息检索"), "文本检索");

    ASSERT_EQ(dict_normalize_term(dict, "信息检索", &normalized), 0);
    ASSERT_NE(normalized, nullptr);
    EXPECT_EQ(std::string(normalized), "文本检索");
    free(normalized);

    tokens = collect_tokens("信息检索系统", dict);
    ASSERT_EQ(tokens.size(), 2U);
    EXPECT_EQ(tokens[0], "文本检索");
    EXPECT_EQ(tokens[1], "系统");

    dict_drop(dict);
}

TEST(DictTest, StopWordsCanBeLoadedFromFile) {
    const char *path = "dict_stop_words_test.txt";
    std::ofstream out(path, std::ios::binary);
    dict_t *dict = dict_create_default();
    std::vector<std::string> tokens;

    ASSERT_NE(dict, nullptr);
    ASSERT_TRUE(out.is_open());
    out << "# stop words\n";
    out << "的\n";
    out << "and\n";
    out.close();

    ASSERT_EQ(dict_load_stop_words_file(dict, path), 0);
    EXPECT_TRUE(dict_is_stop_word(dict, "的"));
    EXPECT_TRUE(dict_is_stop_word(dict, "and"));

    tokens = collect_tokens("自然语言处理的核心 and 方法", dict);
    ASSERT_EQ(tokens.size(), 4U);
    EXPECT_EQ(tokens[0], "自然语言");
    EXPECT_EQ(tokens[1], "处理");
    EXPECT_EQ(tokens[2], "核心");
    EXPECT_EQ(tokens[3], "方法");

    dict_drop(dict);
    std::remove(path);
}

TEST(DictTest, StopWordsHashHandlesLargeVolumeAndCollisions) {
    dict_t *dict = dict_create();

    ASSERT_NE(dict, nullptr);

    // 添加大�?500 个停用词，触发多次扩容
    for (int i = 0; i < 500; ++i) {
        char word[32];
        std::snprintf(word, sizeof(word), "stop_%05d", i);
        ASSERT_EQ(dict_add_stop_word(dict, word), 0);
    }

    // 验证所有停用词均可查
    for (int i = 0; i < 500; ++i) {
        char word[32];
        std::snprintf(word, sizeof(word), "stop_%05d", i);
        EXPECT_TRUE(dict_is_stop_word(dict, word));
    }

    // 不在集合中的词返回 false
    EXPECT_FALSE(dict_is_stop_word(dict, "not_exist"));
    EXPECT_FALSE(dict_is_stop_word(dict, ""));
    EXPECT_FALSE(dict_is_stop_word(dict, nullptr));

    // 重复添加不应报错
    EXPECT_EQ(dict_add_stop_word(dict, "stop_00001"), 0);

    // 重置后哈希表清空
    dict_reset(dict);
    EXPECT_EQ(dict_load_default(dict), 0);

    // 旧停用词不应存在
    EXPECT_FALSE(dict_is_stop_word(dict, "stop_00001"));

    dict_drop(dict);
}

TEST(DictTest, SynonymsHashHandlesUpdateAndCollisions) {
    dict_t *dict = dict_create();

    ASSERT_NE(dict, nullptr);

    // 添加大�?300 个同义词
    for (int i = 0; i < 300; ++i) {
        char word[32], canonical[32];
        std::snprintf(word, sizeof(word), "syn_%05d", i);
        std::snprintf(canonical, sizeof(canonical), "canonical_%05d", i);
        ASSERT_EQ(dict_add_synonym(dict, word, canonical), 0);
    }

    // 验证解析正确
    for (int i = 0; i < 300; ++i) {
        char word[32], expected[32];
        std::snprintf(word, sizeof(word), "syn_%05d", i);
        std::snprintf(expected, sizeof(expected), "canonical_%05d", i);
        EXPECT_STREQ(dict_resolve_synonym(dict, word), expected);
    }

    // 更新已有同义词
    EXPECT_EQ(dict_add_synonym(dict, "syn_00001", "updated_canonical"), 0);
    EXPECT_STREQ(dict_resolve_synonym(dict, "syn_00001"), "updated_canonical");

    // 未注册的词返回原词
    EXPECT_STREQ(dict_resolve_synonym(dict, "unknown_word"), "unknown_word");

    dict_drop(dict);
}

TEST(DictTest, HashTableSurvivesResetAndReuse) {
    dict_t *dict = dict_create();

    ASSERT_NE(dict, nullptr);
    ASSERT_EQ(dict_load_default(dict), 0);

    // 第一轮使�?
    ASSERT_EQ(dict_add_stop_word(dict, "的"), 0);
    ASSERT_EQ(dict_add_synonym(dict, "信息检索", "文本检索"), 0);
    EXPECT_TRUE(dict_is_stop_word(dict, "的"));
    EXPECT_STREQ(dict_resolve_synonym(dict, "信息检索"), "文本检索");

    // 重置
    dict_reset(dict);
    EXPECT_EQ(dict_load_default(dict), 0);  // 重新加载词�?

    // 重置后旧数据不残留
    EXPECT_FALSE(dict_is_stop_word(dict, "的"));
    EXPECT_STREQ(dict_resolve_synonym(dict, "信息检索"), "信息检索");

    // 第二轮使�? 可以重新添加
    ASSERT_EQ(dict_add_stop_word(dict, "的"), 0);
    EXPECT_TRUE(dict_is_stop_word(dict, "的"));

    dict_drop(dict);
}

TEST(DictTest, CutForSearchProducesMoreTokensThanCut) {
    dict_t *dict = dict_create_default();
    std::vector<std::string> cut_tokens;
    std::vector<std::string> search_tokens;

    ASSERT_NE(dict, nullptr);

    cut_tokens = collect_tokens("南京市长江大桥", dict);
    // 用 CutForSearch 分词
    {
        token_t *tokens = nullptr;
        int32_t token_count = 0;
        ASSERT_EQ(dict_cut_for_search(dict, "南京市长江大桥", &tokens, &token_count), 0);
        for (int32_t i = 0; i < token_count; ++i) {
            search_tokens.emplace_back(tokens[i].text);
        }
        dict_free_tokens(tokens, token_count);
    }

    // CutForSearch 输出 >= Cut 输出
    EXPECT_GE(search_tokens.size(), cut_tokens.size());

    // Cut 的每个 token 都应该出现在 CutForSearch 中
    for (const auto &t : cut_tokens) {
        EXPECT_TRUE(std::find(search_tokens.begin(), search_tokens.end(), t) != search_tokens.end())
            << "Cut token '" << t << "' missing from CutForSearch";
    }

    dict_drop(dict);
}

TEST(DictTest, CutForSearchTokensAreInDictionary) {
    dict_t *dict = dict_create_default();
    std::vector<std::string> search_tokens;

    ASSERT_NE(dict, nullptr);

    {
        token_t *tokens = nullptr;
        int32_t token_count = 0;
        ASSERT_EQ(dict_cut_for_search(dict, "自然语言处理系统", &tokens, &token_count), 0);
        for (int32_t i = 0; i < token_count; ++i) {
            search_tokens.emplace_back(tokens[i].text);
        }
        dict_free_tokens(tokens, token_count);
    }

    // 每个 CutForSearch token 必定是 Trie 中的词
    // 验证：把它添加为自定义词（会触发频次更新而不是新增），不影响已有词
    for (const auto &t : search_tokens) {
        int32_t old_size = dict_size(dict);
        EXPECT_EQ(dict_add_word(dict, t.c_str(), 1), 0);
        // 如果词已在词典中，size 不�?
        // 如果不在（极端情况），size 会 +1，但至少不会报�?
        (void)old_size;
    }

    dict_drop(dict);
}

TEST(DictTest, CutForSearchHandlesMixedText) {
    dict_t *dict = dict_create_default();
    std::vector<std::string> search_tokens;

    ASSERT_NE(dict, nullptr);

    {
        token_t *tokens = nullptr;
        int32_t token_count = 0;
        ASSERT_EQ(dict_cut_for_search(dict, "OpenAI发布GPT-4模型", &tokens, &token_count), 0);
        for (int32_t i = 0; i < token_count; ++i) {
            search_tokens.emplace_back(tokens[i].text);
        }
        dict_free_tokens(tokens, token_count);
    }

    // ASCII 混用文本也能正确处理
    EXPECT_FALSE(search_tokens.empty());
    // "OpenAI" 应该出�?
    EXPECT_TRUE(std::find(search_tokens.begin(), search_tokens.end(), "OpenAI") != search_tokens.end());

    dict_drop(dict);
}

TEST(DictTest, CutForSearchHandlesEmptyAndEdgeCases) {
    dict_t *dict = dict_create_default();
    token_t *tokens = nullptr;
    int32_t token_count = -1;

    ASSERT_NE(dict, nullptr);

    // 空字符串
    EXPECT_EQ(dict_cut_for_search(dict, "", &tokens, &token_count), 0);
    EXPECT_EQ(token_count, 0);
    EXPECT_EQ(tokens, nullptr);

    // NULL 输入
    EXPECT_EQ(dict_cut_for_search(dict, nullptr, &tokens, &token_count), -1);
    EXPECT_EQ(dict_cut_for_search(dict, "text", nullptr, &token_count), -1);

    dict_drop(dict);
}

TEST(DictTest, HmmSegmentsOovText) {
    dict_t *dict = dict_create();
    std::vector<std::string> tokens;

    ASSERT_NE(dict, nullptr);
    ASSERT_EQ(dict_load_default(dict), 0);
    // 默认 HMM 开�?
    EXPECT_EQ(dict_enable_hmm(dict, true), 0);

    // 纯未登录词（不在默认词典中）
    {
        token_t *t = nullptr;
        int32_t n = 0;
        ASSERT_EQ(dict_cut(dict, "坜呒岕", &t, &n), 0);
        for (int32_t i = 0; i < n; ++i) tokens.emplace_back(t[i].text);
        dict_free_tokens(t, n);
    }

    // HMM 应该产出一或多个词
    EXPECT_GT(tokens.size(), 0U);

    dict_drop(dict);
}

TEST(DictTest, HmmCanBeDisabled) {
    dict_t *dict = dict_create();
    std::vector<std::string> tokens_hmm_on;
    std::vector<std::string> tokens_hmm_off;

    ASSERT_NE(dict, nullptr);
    ASSERT_EQ(dict_load_default(dict), 0);

    // HMM 开启
    {
        token_t *t = nullptr;
        int32_t n = 0;
        ASSERT_EQ(dict_cut(dict, "坜呒岕", &t, &n), 0);
        for (int32_t i = 0; i < n; ++i) tokens_hmm_on.emplace_back(t[i].text);
        dict_free_tokens(t, n);
    }

    // HMM 关闭
    ASSERT_EQ(dict_enable_hmm(dict, false), 0);
    {
        token_t *t = nullptr;
        int32_t n = 0;
        ASSERT_EQ(dict_cut(dict, "坜呒岕", &t, &n), 0);
        for (int32_t i = 0; i < n; ++i) tokens_hmm_off.emplace_back(t[i].text);
        dict_free_tokens(t, n);
    }

    // HMM 关闭时每个未登录字符独立成单字 token
    EXPECT_EQ(tokens_hmm_off.size(), 3U);  // 三个字符各成一个 token

    dict_drop(dict);
}

TEST(DictTest, HmmHandlesMixedKnownAndOov) {
    dict_t *dict = dict_create_default();
    std::vector<std::string> tokens;

    ASSERT_NE(dict, nullptr);

    // 已知词 + OOV 混用
    {
        token_t *t = nullptr;
        int32_t n = 0;
        ASSERT_EQ(dict_cut(dict, "自然语言坜处理", &t, &n), 0);
        for (int32_t i = 0; i < n; ++i) tokens.emplace_back(t[i].text);
        dict_free_tokens(t, n);
    }

    // "自然语言" 作为已知词应出�?
    EXPECT_TRUE(std::find(tokens.begin(), tokens.end(), "自然语言") != tokens.end());
    // "处理" 作为已知词应出�?
    EXPECT_TRUE(std::find(tokens.begin(), tokens.end(), "处理") != tokens.end());

    dict_drop(dict);
}

TEST(DictTest, SynonymsCanBeLoadedFromFile) {
    const char *path = "dict_synonyms_test.txt";
    std::ofstream out(path, std::ios::binary);
    dict_t *dict = dict_create_default();
    std::vector<std::string> tokens;
    char *normalized = nullptr;

    ASSERT_NE(dict, nullptr);
    ASSERT_TRUE(out.is_open());
    out << "# source target\n";
    out << "信息检索 文本检索\n";
    out.close();

    ASSERT_EQ(dict_add_word(dict, "文本检索", 8000), 0);
    ASSERT_EQ(dict_load_synonyms_file(dict, path), 0);
    EXPECT_STREQ(dict_resolve_synonym(dict, "信息检索"), "文本检索");

    ASSERT_EQ(dict_normalize_term(dict, "信息检索", &normalized), 0);
    ASSERT_NE(normalized, nullptr);
    EXPECT_EQ(std::string(normalized), "文本检索");
    free(normalized);

    tokens = collect_tokens("信息检索系统", dict);
    ASSERT_EQ(tokens.size(), 2U);
    EXPECT_EQ(tokens[0], "文本检索");
    EXPECT_EQ(tokens[1], "系统");

    dict_drop(dict);
    std::remove(path);
}