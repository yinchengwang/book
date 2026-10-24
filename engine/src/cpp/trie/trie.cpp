#include "self_made_cpp/trie.h"


void Trie::deleteTree(TrieNode *node)
{
    if (!node) {
        return;
    }

    for (auto& pair : node->children) {
        deleteTree(pair.second);
    }

    delete node;
}

void Trie::insert(const string& word)
{
    TrieNode *current = root;

    for (char c : word) {
        if (current->children.find(c) == current->children.end()) {
            current->children[c] = new TrieNode();
        }
        current = current->children[c];
    }

    current->isEndOfWord = true;
}

bool Trie::search(const string& word)
{
    TrieNode *current = root;

    for (char c : word) {
        if (current->children.find(c) == current->children.end()) {
            return false;
        }
        current = current->children[c];
    }

    return current->isEndOfWord;
}

bool Trie::startsWith(const string& prefix) {
    TrieNode *current = root;
    
    for (char c : prefix) {
        if (current->children.find(c) == current->children.end()) {
            return false;
        }
        current = current->children[c];
    }

    return true;
}

vector<string> Trie::getAllWordsWithPrefix(const string& prefix)
{
    vector<string> result;
    TrieNode *current = root;

    // 先找到前缀的最后一个节点
    for (char c : prefix) {
        if (current->children.find(c) == current->children.end()) {
            return result;  // 前缀不存在，返回空列表
        }
        current = current->children[c];
    }

    // 收集所有以该前缀开头的单词
    collectWords(current, prefix, result);
    return result;
}

void Trie::collectWords(TrieNode *node, string prefix, vector<string>& result)
{
    if (node->isEndOfWord) {
        result.push_back(prefix);
    }

    for (auto& pair : node->children) {
        collectWords(pair.second, prefix + pair.first, result);
    }
}