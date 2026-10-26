#include <iostream> 

int main()
{
    // 本程序故意制造 WA：能正常编译、运行和结束，但答案错误。
    long long a = 0; // 保存第一个整数
    long long b = 0; // 保存第二个整数

    std::cin >> a >> b; // 与 AC 程序读取同一份测试输入
    std::cout << a + b + 1 << '\n'; // 故意多加 1；测试点 1 将输出 4，而标准答案是 3

    return 0; // 正常结束，使错误归因于输出不一致，而非运行异常
}