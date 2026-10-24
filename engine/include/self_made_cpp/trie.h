#ifndef TRIE_CPP_H
#define TRIE_CPP_H

#include <unordered_map>
#include <vector>
#include <string>

using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

class TrieNode {
public:
    unordered_map<char, TrieNode *> children;
    bool isEndOfWord;  // 标记是否是单词的结尾

    TrieNode() : isEndOfWord(false) {}
};


class Trie {
private:
    TrieNode* root;

    // 递归删除树的辅助函数
    void deleteTree(TrieNode* node);

public:
    Trie() {
        root = new TrieNode();
    }

    ~Trie() {
        deleteTree(root);
    }

    // 插入单词
    void insert(const string& word);

    // 查找完整单词
    bool search(const string& word);

    // 查找前缀
    bool startsWith(const string& prefix);

    // 查找所有以给定前缀开头的单词
    vector<string> getAllWordsWithPrefix(const string& prefix);

private:
    // 辅助函数：收集从当前节点开始的所有单词
    void collectWords(TrieNode* node, string prefix, vector<string>& result);
};

#ifdef __cplusplus
}
#endif // extern "C"

#endif // #define TRIE_CPP_H
