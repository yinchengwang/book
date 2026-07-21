#include <iostream>
#include <string>

using namespace std;

void StringBase()
{
    string s = "Hello, World!";

    // 长度（length 和 size 完全一样）
    // 输出：13
    cout << s.length() << endl;
    // 输出：13
    cout << s.size() << endl;

    // 通过下标访问单个字符
    // 输出：H
    cout << s[0] << endl;

    // 截取子串：substr(起始位置, 长度)
    // 输出：Hello
    cout << s.substr(0, 5) << endl;

    // 查找子串：find 返回首次出现的位置
    // 输出：7
    cout << s.find("World") << endl;

    // 找不到返回 string::npos
    if (s.find("xyz") == string::npos) {
        // 输出：not found
        cout << "not found" << endl;
    }

    // 字符串比较：直接用 == 就行
    string a = "hello";
    string b = "hello";
    // 输出：1
    cout << (a == b) << endl;

    // 字符串拼接：用 + 就行
    string c = a + " world";
    // 输出：hello world
    cout << c << endl;

    // 遍历每个字符
    // 输出：h e l l o
    for (char ch : a) {
        cout << ch << " ";
    }
    cout << endl;
}

void NumberConversion()
{
    // 字符串转 int
    int n = stoi("123");
    // 输出：123
    cout << n << endl;

    // 字符串转 long long
    long long big = stoll("1234567890123");
    // 输出：1234567890123
    cout << big << endl;

    // 数字转字符串
    string s = to_string(42);
    // 输出：42
    cout << s << endl;

    string s2 = to_string(3.14);
    // 输出：3.140000
    cout << s2 << endl;
}

void Conversion()
{
    // isdigit：是否为数字字符 '0'-'9'
    // 输出：1
    cout << (isdigit('3') != 0) << endl;
    // 输出：0
    cout << (isdigit('a') != 0) << endl;

    // isalpha：是否为字母
    // 输出：1
    cout << (isalpha('a') != 0) << endl;

    // toupper / tolower：大小写转换
    // 输出：A
    cout << (char)toupper('a') << endl;
    // 输出：a
    cout << (char)tolower('A') << endl;

    // 常用技巧：字母转数字索引（a=0, b=1, ...）
    // 这在算法题中非常常见，比如用数组代替哈希表统计字母频率
    char letter = 'd';
    int index = letter - 'a';
    // 输出：3
    cout << index << endl;
}

int main()
{
    StringBase();
    NumberConversion();
    Conversion();

    return 0;
}