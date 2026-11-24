#include <iostream>
#include <string>

// 以下程序用于检查输出超限
// main没有参数。
// 逻辑：循环输出字符块且不自行结束，预期由判题引擎在输出超限后终止。
int main()
{
    std::string block(4096, 'x');

    while (true)
    {
        std::cout.write(block.c_str(), block.size());
        std::cout.flush();
    }

    return 0;
}
