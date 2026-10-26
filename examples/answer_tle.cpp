int main()
{
    // volatile 使编译器保留循环中的计数操作，
    // 避免优化时把这个用于测试的空循环改掉。
    volatile unsigned long long counter = 0;

    // 程序持续运行，不会自行返回。
    // B 的判题引擎应在 P001 的时限到达后结束该进程，并返回 TLE。
    while (true)
    {
        ++counter; // 每轮执行一次计数；无标准输出
    }

    return 0; // 正常情况下执行不到这里
}