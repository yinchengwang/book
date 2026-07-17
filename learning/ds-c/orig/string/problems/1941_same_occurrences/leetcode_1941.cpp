#include <iostream>
#include <string>

using namespace std;

class Solution {
public:
    bool areOccurrencesEqual(string s) {
        int map[26] = {0};

        for (int i = 0; i < s.size(); i++) {
            map[s[i] - 'a']++;
        }

        int cnt = map[s[0] - 'a'];
        for (int i = 0; i < 26; i++) {
            if (map[i] && map[i] != cnt) {
                return false;
            }
        }

        return true;
    }
};

int main()
{
    Solution solution;

    string s = "abacbc";
    bool res = solution.areOccurrencesEqual(s);
    cout << "res is: " << res << endl;

    s = "aaabb";
    res = solution.areOccurrencesEqual(s);
    cout << "res is: " << res << endl;

    return 0;
}