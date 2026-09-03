#include <iostream>
#include <string>
using namespace std;

class Solution {
public:
    bool isPrefix(string sentence, int start, int end, string searchWord)
    {
        for (int i = 0; i < searchWord.size(); i++) {
            if (start + i > end || sentence[start + i] != searchWord[i]) {
                return false;
            }
        }

        return true;
    }
    int isPrefixOfWord(string sentence, string searchWord) {
        int start = 0, end = 0, idx = 1;
        size_t len = sentence.size();
        while (start < len) {
            while (end <len && sentence[end] != ' ') {
                end++;
            }

            if (isPrefix(sentence, start, end, searchWord)) {
                return idx;
            }

            idx++;
            end++;
            start = end;
        }

        return -1;
    }
};