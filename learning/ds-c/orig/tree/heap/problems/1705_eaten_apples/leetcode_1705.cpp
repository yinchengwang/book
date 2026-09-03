#include<iostream>
#include<vector>
#include<queue>
#include<utility>

using namespace std;

class Solution {
public:
typedef pair<int, int> ap;
    int eatenApples(vector<int>& apples, vector<int>& days) {
        int ans = 0;
        priority_queue<ap, vector<ap>, greater<ap>> pq;
        int n = apples.size();
        int i = 0;
        while (i < n) {
            while (!pq.empty() && pq.top().first <= i) {
                pq.pop();
            }

            int rotten_day = i + days[i];
            int count = apples[i];
            if (count > 0) {
                pq.emplace(rotten_day, count);
            }

            if (!pq.empty()) {
                ap cur = pq.top();
                pq.pop();
                cur.second--;
                if (cur.second > 0) {
                    pq.emplace(cur.first, cur.second);
                }

                ans++;
            }

            i++;
        }

        while (!pq.empty()) {
            while (!pq.empty() && pq.top().first <= i) {
                pq.pop();
            }

            if (pq.empty()) {
                break;
            }

            ap cur = pq.top();
            pq.pop();
            int num = min(cur.first - i, cur.second);
            ans += num;
            i += num;
        }

        return ans;
    }
};