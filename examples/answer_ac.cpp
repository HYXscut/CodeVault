#include <iostream> 

int main()
{
    // 本程序对应 P001「A+B 求和」。
    // main 没有参数；正常完成时返回 0。
    long long a = 0; // 保存输入的第一个整数
    long long b = 0; // 保存输入的第二个整数

    std::cin >> a >> b; // 按题目格式读取两个由空格分隔的整数
    std::cout << a + b << '\n'; // 输出正确的和；'\n' 保留标准答案的末尾换行

    return 0; // 告诉判题器程序正常结束
}