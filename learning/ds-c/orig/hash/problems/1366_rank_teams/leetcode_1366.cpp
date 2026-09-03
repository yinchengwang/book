#include<iostream>
#include<string>
#include<vector>
#include<unordered_map>
#include<algorithm>

using namespace std;

// 难度一般, 需要多读懂题目意思即可
class Solution {
public:
    string rankTeams(vector<string>& votes) {
        string ans;
        unordered_map<char, vector<int>> team_scores;
        for (char team: votes[0]) {
            team_scores[team].resize(votes[0].size());
        }

        for (string vote: votes) {
            for (int i = 0; i < vote.size(); i++) {
                team_scores[vote[i]][i]++;
            }
        }

        using CVIP = pair<char, vector<int>>;
        vector<CVIP> result(team_scores.begin(), team_scores.end());

        sort(result.begin(), result.end(), [](const CVIP &lhs, const CVIP &rhs) {
            return lhs.second > rhs.second || (lhs.second == rhs.second && lhs.first < rhs.first);
        });

        for (auto [ch, score]: result) {
            ans += ch;
        }

        return ans;
    }
};