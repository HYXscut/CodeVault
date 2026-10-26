#include <iostream> 

int main()
{
    // 功能：制造一次可重复的非零退出，供 B 检查 RE 分支。
    // 参数：main 不接收命令行参数。
    // 步骤：读取 P001 的两个整数，向标准错误输出说明原因，最后返回非零值；本程序不向标准输出写答案。
    long long a = 0; // 保存测试输入中的第一个整数
    long long b = 0; // 保存测试输入中的第二个整数

    std::cin >> a >> b; // 读取与其他四份样例相同的 1.in
    std::cerr << "intentional non-zero exit\n"; // 供 B 检查 stderr 捕获
    return 7; // 非零退出码；7 便于在检查记录中辨认
}