#include <algorithm>
#include <deque>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

// 01
// 某小区宣传其在冬季采暖期间，提供室内温度保持在18 -
// 24摄氏度之间，且温差在4摄氏度内的恒温服务。为了评价该小区冬季采暖温控效果，现按固定时间间隔对室内温度进行连续N次采样，采样的室内温度按照从0开始升序的索引号依次记录。通过计算采样的室内温度在小区承诺恒温范围内的持续时间，即持续时间内的采样室内温度值T均满足
// 18≤T≤24，且持续时间内的温差（最大温度max -
// 最低温度min）≤4，持续时间越长，采暖温控效果越好。请输出最大的持续时间开始点和结束点的采样的室内温度索引号，如果存在多个最大持续时间，按持续时间的开始点采样的室内温度索引号升序，逐行输出。

// 6
// 16 18 20 25 23 20

// 1 2
// 4 5

// 3
// 16 18 16

// 1 1

// 13
// 16 18 20 21 22 24 26 25 23 21 21 20 19

// 8 12

// 11
// 16 17 18 19 20 21 22 21 22 24 26

// 2 8

vector<vector<int>> findLongestTemperatureWindows(const vector<int> &temps)
{
    int n = temps.size();
    if (n == 0)
        return {};

    deque<int> maxQ, minQ;
    int left = 0;
    int maxLen = 0;
    vector<vector<int>> result;

    for (int right = 0; right < n; right++) {
        int t = temps[right];

        // 如果温度不在有效范围内，重置窗口
        if (t < 18 || t > 24) {
            left = right + 1;
            maxQ.clear();
            minQ.clear();
            continue;
        }

        // 维护最大值队列（递减）
        while (!maxQ.empty() && temps[maxQ.back()] <= t) {
            maxQ.pop_back();
        }
        maxQ.push_back(right);

        // 维护最小值队列（递增）
        while (!minQ.empty() && temps[minQ.back()] >= t) {
            minQ.pop_back();
        }
        minQ.push_back(right);

        // 调整左边界直到温差<=4
        while (!maxQ.empty() && !minQ.empty() && temps[maxQ.front()] - temps[minQ.front()] > 4) {
            if (maxQ.front() == left) {
                maxQ.pop_front();
            }
            if (minQ.front() == left) {
                minQ.pop_front();
            }
            left++;
        }

        int currentLen = right - left + 1;
        if (currentLen > maxLen) {
            maxLen = currentLen;
            result.clear();
            result.push_back({left, right});
        } else if (currentLen == maxLen && maxLen > 0) {
            result.push_back({left, right});
        }
    }

    // 按起始索引排序
    sort(result.begin(), result.end(), [](const vector<int> &a, const vector<int> &b) { return a[0] < b[0]; });

    return result;
}

// 03

int levenshtein_distance(const string &s1, const string &s2) {
    int len1 = s1.length();
    int len2 = s2.length();

    vector<vector<int>> dp(len1 + 1, vector<int>(len2 + 1));

    for (int i = 0; i <= len1; i++) {
        dp[i][0] = i;
    }
    for (int j = 0; j <= len2; j++) {
        dp[0][j] = j;
    }

    for (int i = 1; i <= len1; i++) {
        for (int j = 1; j <= len2; j++) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            dp[i][j] = min({dp[i - 1][j] + 1, dp[i][j - 1] + 1, dp[i - 1][j - 1] + cost});
        }
    }

    return dp[len1][len2];
}

string process_commands(int D, const vector<string> &commands, const string &user_input) {
    // Check for exact match first
    for (const auto &cmd : commands) {
        if (cmd == user_input) {
            return user_input;
        }
    }

    // Calculate distances and find candidates
    vector<pair<string, int>> candidates;

    for (const auto &cmd : commands) {
        int distance = levenshtein_distance(user_input, cmd);
        if (distance <= D) {
            candidates.emplace_back(cmd, distance);
        }
    }

    if (candidates.empty()) {
        return "None";
    }

    // Sort candidates by distance and then alphabetically
    sort(candidates.begin(), candidates.end(), [](const pair<string, int> &a, const pair<string, int> &b) {
        if (a.second != b.second) {
            return a.second < b.second;
        }
        return a.first < b.first;
    });

    // Build result string with all candidates having the smallest distance
    int min_distance = candidates[0].second;
    stringstream result;
    bool first = true;

    for (const auto &candidate : candidates) {
        if (candidate.second == min_distance) {
            if (!first) {
                result << "\n";
            }
            result << candidate.first;
            first = false;
        }
    }

    return result.str();
}