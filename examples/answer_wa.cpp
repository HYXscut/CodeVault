#include <iostream>

// 以下程序用于检查答案错误WA
// main没有参数。
// 逻辑：读取两个整数，故意把和多加1，正常返回0。
int main()
{

    long long a = 0;
    long long b = 0;

    std::cin >> a >> b;
    std::cout << a + b + 1 << '\n';

    return 0;
}
