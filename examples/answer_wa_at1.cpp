#include <iostream>

// 以下程序用于检查首个测试点答错后停止
// main没有参数。
// 逻辑：读取两个整数并把和多加1，正常返回0，预期第一点WA。
int main()
{
    long long a, b;

    std::cin >> a >> b;
    std::cout << a + b + 1 << '\n';

    return 0;
}
