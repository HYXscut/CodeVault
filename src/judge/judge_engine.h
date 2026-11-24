// 本地判题引擎模块：负责异步编译学生代码、逐点运行测试并汇总结果
#ifndef CODEJUDGE_JUDGE_ENGINE_H
#define CODEJUDGE_JUDGE_ENGINE_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <string>
#include <vector>

#include "common/types.h"

class QTemporaryDir;
class QTimer;

// 本地判题引擎类，公有继承自 QObject，靠 Qt 的信号槽实现异步编译和逐点评测，不卡住主界面。
// 主要属性：stage_ 表示当前阶段，compilerPath_ 和 compileTimeoutMs_ 控制编译，
// process_/timer_/workDir_/runClock_ 管理子进程、计时和临时目录，
// problem_/tests_/currentIndex_/passedCount_/executedCount_/totalElapsedMs_/result_ 记录本次任务数据与结果，
// timedOut_/outputLimitExceeded_/ioError_/taskId_/stepId_/timerStepId_/stepSettled_ 用于超时、超限和防误触控制，
// compilerOutput_/actualOutput_/runtimeError_ 缓存进程输出。
// 对外提供 startJudge()、getResult()、isBusy()、setCompileTimeoutMs()，跑完发出 judgeFinished() 信号；
// 内部通过编译、运行、进程回调和几个辅助函数串联完整判题流程。
class JudgeEngine : public QObject
{
    Q_OBJECT

  public:
    // 构造函数：保存编译器路径 compilerPath，初始化内部状态和信号槽，parent 交给 QObject 管理
    JudgeEngine(std::string compilerPath, QObject *parent = nullptr);

    // 析构函数：停掉可能还在跑的子进程，并释放临时工作目录
    ~JudgeEngine();

    // 发起一次异步判题；problem 是题目，sourcePath 是源码路径，tests 是测试点列表；
    // 无法开始时把原因写入 error 并返回 false，接收成功返回 true
    bool startJudge(Problem problem, std::string sourcePath, std::vector<TestCase> tests, std::string &error);

    // 返回最近一次判题的完整结果
    JudgeResult getResult();

    // 判断当前是不是正在判题，忙碌返回 true
    bool isBusy() const;

    // 设置编译超时上限 timeoutMs（毫秒，默认 30000），主要方便测试时缩短等待时间
    void setCompileTimeoutMs(int timeoutMs);

  signals:
    // 判题全部结束、结果写完且临时目录清理好后发出，通知界面读取结果
    void judgeFinished();

  private slots:
    // 启动编译和启动单个测试点运行的槽函数
    void startCompile();
    void startRun();

    // 响应 QProcess 和 QTimer 各种事件的槽函数
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onTimeout();
    void onProcessStarted();
    void onStdoutReady();
    void onStderrReady();

  private:
    // 判题所处的四个阶段：空闲、编译中、运行测试点中、收尾清理中
    static const int STAGE_IDLE = 0;      // 空闲
    static const int STAGE_COMPILING = 1; // 编译中
    static const int STAGE_RUNNING = 2;   // 运行中
    static const int STAGE_CLEANING = 3;  // 收尾清理中

    int stage_;                // 当前所处阶段
    std::string compilerPath_; // g++ 编译器路径
    int compileTimeoutMs_;     // 编译超时上限（毫秒，默认 30000）

    QProcess *process_;      // 运行 g++ 和学生程序的外部进程对象
    QTimer *timer_;          // 编译与单点运行共用的超时计时器
    QTemporaryDir *workDir_; // 本次判题专用的临时工作目录
    QElapsedTimer runClock_; // 统计单个测试点运行耗时的计时器

    Problem problem_;             // 当前题目副本
    std::vector<TestCase> tests_; // 本次判题的全部测试点（按 1..testCount 排列）
    int currentIndex_;            // 当前正在跑的测试点下标（从 0 开始）
    int passedCount_;             // 已通过（AC）的测试点个数
    int executedCount_;           // 已实际启动运行的测试点个数
    long long totalElapsedMs_;    // 已跑完测试点的累计耗时（不含编译时间）
    std::string sourcePath_;      // 学生原始源码路径
    JudgeResult result_;          // 本次判题的结果结构体

    bool timedOut_;              // 当前步骤是否发生超时
    bool outputLimitExceeded_;   // 输出是否超过 16 MiB 上限
    bool ioError_;               // 是否发生进程管道读写错误
    std::string ioErrorMessage_; // 管道读写出错的说明文字
    std::string cleanupError_;   // 清理编译器进程树失败时的说明文字

    int taskId_;       // 任务序号，每次 startJudge 自增
    int stepId_;       // 步骤序号，每次开始编译或跑新测试点时自增
    int timerStepId_;  // 当前计时器绑定的步骤号，防止旧点超时误伤新点
    bool stepSettled_; // 当前编译或测试点是否已结算，防止重复处理

    QByteArray compilerOutput_; // 暂存编译器输出
    QByteArray actualOutput_;   // 暂存学生程序标准输出
    QByteArray runtimeError_;   // 暂存学生程序错误输出

    // 统一收尾：确认进程退出、删掉临时目录、切回空闲并发出 judgeFinished 信号
    void finishJudge();

    // 编译超时时用 taskkill 按 PID 结束 g++ 及其拉起的子进程
    void terminateCompileTree();

    // 读取子进程输出（fromStderr 区分错误输出还是标准输出），并检查是否超过 16 MiB
    void collectOutput(bool fromStderr);

    // 分析编译输出 compilerOutput，判断是不是工具链损坏或组件缺失（区别于代码语法错）
    bool isToolchainFailure(const std::string &compilerOutput) const;
};

#endif