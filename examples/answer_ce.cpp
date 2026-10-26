#include <iostream> 

int main()
{
    // 本文件故意制造编译错误，用于检查判题器是否返回 CE。
    long long a = 0; // 第一个输入整数
    long long b = 0; // 第二个输入整数

    std::cin >> a >> b; // 读取 P001 的两个整数
    std::cout << a + b << '\n' // 故意漏写分号，编译器应在这里报语法错误

    return 0;
}