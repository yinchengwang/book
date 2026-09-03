#include<iostream>
#include<set>

using namespace std;

class MyCalendar {
    set<pair<int, int>> booked;
public:
    MyCalendar() {}
    
    bool book(int startTime, int endTime) {
        auto it = booked.lower_bound({endTime, 0});
        if (it == booked.begin() || (--it)->second <= startTime) {
            booked.emplace(startTime, endTime);
            return true;
        }
        return false;
    }
};


int main()
{
    MyCalendar* obj = new MyCalendar();
    bool param_1 = obj->book(10, 20);
    param_1 = obj->book(15, 25);
    param_1 = obj->book(20, 30);
}