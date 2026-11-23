#include <iostream>

int main()
{
    long long a, b;

    std::cin >> a >> b;

    if (a == 0) // 五个测试点中，只有第三点满足这个条件
    {
        std::cout << a + b + 1 << '\n'; // 故意让答案多 1
    }
    else
    {
        std::cout << a + b << '\n';
    }

    return 0; // 正常退出，便于单独检查答案错误 WA
}