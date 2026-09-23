#include <gtest/gtest.h>
#include "mmrag/query_classifier.h"
#include "mmrag/pipeline.h"

using namespace mmrag;

TEST(QueryClassifier, ChatGreeting) {
    RuleBasedQueryClassifier c;
    EXPECT_EQ(c.classify("你好"), QueryType::CHAT);
    EXPECT_EQ(c.classify("谢谢你"), QueryType::CHAT);
}

// 现存 bug：".*吗$" 把所有疑问句误判为 CHAT，本测试驱动修复
TEST(QueryClassifier, QuestionMarkSentenceIsNotChat) {
    RuleBasedQueryClassifier c;
    EXPECT_NE(c.classify("这个配置生效了吗"), QueryType::CHAT);
}

TEST(QueryClassifier, OutOfScopeCommand) {
    RuleBasedQueryClassifier c;
    EXPECT_EQ(c.classify("帮我写个周报"), QueryType::OUT_OF_SCOPE);
    EXPECT_EQ(c.classify("讲个笑话"), QueryType::OUT_OF_SCOPE);
    EXPECT_EQ(c.classify("今天天气怎么样"), QueryType::OUT_OF_SCOPE);
}

TEST(QueryClassifier, KnowledgeQuestionProceeds) {
    RuleBasedQueryClassifier c;
    QueryType t = c.classify("什么是HNSW索引");
    EXPECT_NE(t, QueryType::CHAT);
    EXPECT_NE(t, QueryType::OUT_OF_SCOPE);
}

TEST(QueryType, OutOfScopeStringRoundTrip) {
    EXPECT_EQ(query_type_to_string(QueryType::OUT_OF_SCOPE), "out_of_scope");
    EXPECT_EQ(string_to_query_type("out_of_scope"), QueryType::OUT_OF_SCOPE);
}

TEST(QueryType, OutOfScopeNeedsNoRetrieval) {
    RuleBasedQueryClassifier c;
    EXPECT_FALSE(c.needs_retrieval(QueryType::OUT_OF_SCOPE));
    EXPECT_FALSE(c.needs_retrieval(QueryType::CHAT));
    EXPECT_TRUE(c.needs_retrieval(QueryType::FACTUAL));
}
