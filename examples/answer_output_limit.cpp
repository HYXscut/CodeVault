#include <iostream>
#include <string>

int main()
{
    std::string block(4096, 'x'); // 创建包含 4096 个 x 的字符串

    while (true)
    {
        std::cout.write(block.c_str(), block.size()); // 每轮输出一块数据
        std::cout.flush(); // 将缓冲区中的数据送出
    }

    return 0;
}