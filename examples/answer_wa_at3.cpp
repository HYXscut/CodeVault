#include <iostream>

// 以下程序用于检查第三个测试点答错后停止
// main没有参数。
// 逻辑：只有首数为0时把和多加1，正常返回0；五点题的第三点满足此条件。
int main()
{
    long long a, b;

    std::cin >> a >> b;

    if (a == 0) // 五点题中只有第三点满足
    {
        std::cout << a + b + 1 << '\n';
    }
    else
    {
        std::cout << a + b << '\n';
    }

    return 0;
}
