#include <iostream>

// 以下程序用于检查多测试点全部通过
// main没有参数。
// 逻辑：读取两个整数并输出它们的和，每次正常返回0。
int main()
{
    long long a, b;

    std::cin >> a >> b;
    std::cout << a + b << '\n';

    return 0;
}
