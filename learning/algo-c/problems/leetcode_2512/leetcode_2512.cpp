/*
 * @Author: yinchengwang
 * @Date: 2025-02-08 20:26:33
 * @LastEditors: yinchengwang
 * @LastEditTime: 2025-02-08 20:42:07
 * @Description: leetcode 2512 https://leetcode.cn/problems/reward-top-k-students/description/
 * @FilePath: \book\leetcode\leetcode_2512\leetcode_2512.cpp
 */

#include <iostream>
#include <queue>
#include <map>
#include <vector>
#include <string>
#include <algorithm>
#include <functional>
#include <unordered_map>

using namespace std;

class Solution {
    public:
        vector<int> topStudents(vector<string> &positive_feedback, vector<string> &negative_feedback,
            vector<string> &report, vector<int> &student_id, int k)
        {
            unordered_map<string, int> words_p;
            unordered_map<string, int> words_n;

            for (auto &positive: positive_feedback) {
                words_p[positive] = 3;
            }

            for (auto &negative: negative_feedback) {
                words_n[negative] = -1;
            }

            priority_queue<pair<int, int>, vector<pair<int, int>>,
                function<bool(pair<int, int>, pair<int, int>)>> pq(
                    [](const pair<int, int> &lhs, const pair<int, int> &rhs) {
                        return lhs.first < rhs.first || (lhs.first == rhs.first && lhs.second > rhs.second);
                    }
                );

            int report_size = report.size();
            for (int i = 0; i < report_size; i++) {
                int score = 0;
                int j = 0;
                string word = "";
                int word_size = report[i].size();
                while (j < word_size) {
                    while (j < word_size && report[i][j] != ' ') {
                        word += report[i][j];
                        j++;
                    }

                    if (words_n.count(word)) {
                        // cout << "words_n " << word << endl;
                        score -= 1;
                    } else if (words_p.count(word)) {
                        // cout << "words_p " << word << endl;
                        score += 3;
                    }

                    j++;
                    word = "";
                }

                pq.push({score, student_id[i]});
            }

            vector<int> top_k;
            while (k-- && !pq.empty()) {
                // cout << "id: " << pq.top().second << " score: " << pq.top().first << endl;
                top_k.push_back(pq.top().second);
                pq.pop();
            }

            return top_k;
        }
    };

int main()
{
    Solution solution;

    vector<string> positive_feedback = { "smart","brilliant","studious" };
    vector<string> negative_feedback = { "not" };
    vector<string> report = { "this student is studious", "the student is smart" };
    vector<int> student_id = { 1, 2 };
    int k = 2;

    vector<int> top_k = solution.topStudents(positive_feedback, negative_feedback, report, student_id, k);
    cout << "topk(" << k << "): ";
    for (auto &k: top_k) {
        cout << k << " ";
    }
    cout << endl;

    positive_feedback = {"smart", "brilliant", "studious"};
    negative_feedback = {"not"};
    report = { "this student is not studious","the student is smart" };
    student_id = { 1, 2 };
    k = 2;

    top_k = solution.topStudents(positive_feedback, negative_feedback, report, student_id, k);
    cout << "topk(" << k << "): ";
    for (auto &k: top_k) {
        cout << k << " ";
    }
    cout << endl;

    positive_feedback = {"fkeofjpc", "qq", "iio"};
    negative_feedback = {"jdh", "khj", "eget", "rjstbhe", "yzyoatfyx", "wlinrrgcm"};
    report = {
        "rjstbhe eget kctxcoub urrmkhlmi yniqafy fkeofjpc iio yzyoatfyx khj iio",
        "gpnhgabl qq qq fkeofjpc dflidshdb qq iio khj qq yzyoatfyx",
        "tizpzhlbyb eget z rjstbhe iio jdh jdh iptxh qq rjstbhe",
        "jtlghe wlinrrgcm jnkdbd k iio et rjstbhe iio qq jdh",
        "yp fkeofjpc lkhypcebox rjstbhe ewwykishv egzhne jdh y qq qq",
        "fu ql iio fkeofjpc jdh luspuy yzyoatfyx li qq v",
        "wlinrrgcm iio qq omnc sgkt tzgev iio iio qq qq",
        "d vhg qlj khj wlinrrgcm qq f jp zsmhkjokmb rjstbhe"
    };
    student_id = {96537918, 589204657, 765963609, 613766496, 43871615, 189209587, 239084671, 908938263};
    k = 3;

    top_k = solution.topStudents(positive_feedback, negative_feedback, report, student_id, k);
    cout << "topk(" << k << "): ";
    for (auto &k: top_k) {
        cout << k << " ";
    }
    cout << endl;

    return 0;
}