#include "gtest/gtest.h"

#include "self_made_cpp/trie.h"

class TrieUnitsTest : public::testing::Test {
public:

protected:
    // 每个测试用例执行前调用
    void SetUp() override {

    }

    // 每个测试用例执行后调用
    void TearDown() override {
        // 清理代码
    }

    // 整个测试类执行前调用
    static void SetUpTestCase() {
        // 初始化代码
        printf("Setting up for the BstUnitsTest entire test class\n");
    }

    // 整个测试类执行后调用
    static void TearDownTestCase() {
        // 清理代码
        printf("Tearing down after the BstUnitsTest entire test class\n");
    }
};

TEST_F(TrieUnitsTest, Trie_test)
{
    Trie trie = Trie();

    string str = "这是测试字符串";
    trie.insert(str);

    string prefix = "这是";
    bool isStartWith = trie.startsWith(prefix);
    EXPECT_EQ(isStartWith, true);
}