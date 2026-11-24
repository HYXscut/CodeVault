#include <iostream>

// 以下程序用于检查运行错误RE
// main没有参数。
// 逻辑：读取两个整数，输出错误说明并返回7，不向标准输出写答案。
int main()
{

    long long a = 0;
    long long b = 0;

    std::cin >> a >> b;
    std::cerr << "intentional non-zero exit\n";
    return 7;
}
