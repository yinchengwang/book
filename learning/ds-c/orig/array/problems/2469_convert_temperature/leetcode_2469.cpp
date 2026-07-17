#include <iostream>
#include <vector>
#include <algorithm>

using namespace std;

class Solution {
public:
    vector<double> convertTemperature(double celsius) {
        vector<double> res;
        double kelvin = celsius + 273.15;
        double fahrenheit = celsius * 1.80 + 32.00;

        res.push_back(kelvin);
        res.push_back(fahrenheit);

        return res;
    }
};

int main()
{
    double celsius = 36.50;
    Solution solution;
    vector<double> res = solution.convertTemperature(celsius);
    for (auto i : res) {
        cout << i << endl;

    }

    return 0;
}