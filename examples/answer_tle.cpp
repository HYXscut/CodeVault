// 以下程序用于检查运行超时TLE
// main没有参数。
// 逻辑：循环计数且不自行结束，预期由判题引擎在超时后终止。
int main()
{

    volatile unsigned long long counter = 0; // 保留计数操作，避免测试循环被优化掉

    while (true)
    {
        ++counter;
    }

    return 0;
}
