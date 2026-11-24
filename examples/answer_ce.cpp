#include <iostream>

// 以下程序用于检查编译错误CE
// main没有参数。
// 逻辑：输出语句故意缺少分号，预期无法编译，不会执行main。
int main()
{

    long long a = 0;
    long long b = 0;

    std::cin >> a >> b;
    std::cout << a + b << '\n' // 故意漏写分号，保留这个错误

    return 0;
}
