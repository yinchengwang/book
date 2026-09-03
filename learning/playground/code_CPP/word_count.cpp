/* 读取一个字符串中的单词数, 相邻的重复单词数, 整个字符串中的重复单词数 */

#include<iostream>
#include<string>

using namespace std;

int word_count(string str)
{
    int cnt = 0;
    int start = 0, end = 0;
    size_t len = str.size();
    while (start < len) {
        while (end < len && str[end] != ' ') {
            end++;
        }

        if (start != end) {
            cnt++;
        }

        end++;
        start = end;
    }

    return cnt;
}

bool is_same_word(string lhs, string rhs)
{
    if (lhs.size() != rhs.size()) {
        return false;
    }

    size_t len = lhs.size();
    for (int i = 0; i < len; i++) {
        if (lhs[i] != rhs[i]) {
            return false;
        }
    }

    return true;
}

int consecutive_duplicate_words(string str)
{
    string previous_word = " ";
    bool is_consecutive = false;
    int start = 0, end = 0, res = 0;
    size_t len = str.size();
    while (start < len) {
        while (end < len && str[end] != ' ') {
            end++;
        }

        string curr_word = str.substr(start, end - start);
        if (is_same_word(previous_word, curr_word) && !is_consecutive) {
            is_consecutive = true;
            res++;
        } else {
            previous_word = curr_word;
            is_consecutive = false;
        }

        end++;
        start = end;
    }

    return res;
}

int main()
{
    string str = "She she laughted He He He because what he did did not not look very very good good";
    int cnt = word_count(str);
    cout << "str word count: " << cnt << endl;

    int consecutive_duplicate_cnt = consecutive_duplicate_words(str);
    cout << "consecutive duplicate words: " << consecutive_duplicate_cnt << endl;
    return 0;
}