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

// 判题引擎类：负责异步编译学生代码、运行测试点并收集判题结果
class JudgeEngine : public QObject
{
    Q_OBJECT

  public:
    // 构造函数：记录编译器路径并初始化内部状态（不立即开始判题）
    JudgeEngine(std::string compilerPath, QObject *parent = nullptr);

    // 析构函数：释放临时目录指针等资源
    ~JudgeEngine();

    // 异步启动一次判题任务：成功接受返回 true，否则返回 false 并写入 error
    bool startJudge(Problem problem, std::string sourcePath, std::vector<TestCase> tests, std::string &error);

    // 获取最近一次判题的结果
    JudgeResult getResult();

    // 查询当前是否正在执行判题任务
    bool isBusy() const;

    // 调整编译超时上限（默认 30000 毫秒；仅用于测试时快速验证编译超时清理）
    void setCompileTimeoutMs(int timeoutMs);

  signals:
    // 判题完成信号：保存好结果并完成清理后发出
    void judgeFinished();

  private slots:
    // 发起编译与发起运行的处理函数
    void startCompile();
    void startRun();

    // QProcess 与 QTimer 触发的槽函数
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onTimeout();
    void onProcessStarted();
    void onStdoutReady();
    void onStderrReady();

  private:
    // 判题阶段常量（用普通整型常量区分当前阶段）
    static const int STAGE_IDLE = 0;      // 空闲状态
    static const int STAGE_COMPILING = 1; // 正在编译
    static const int STAGE_RUNNING = 2;   // 正在运行测试点
    static const int STAGE_CLEANING = 3;  // 正在收尾清理

    int stage_;                 // 当前所处阶段
    std::string compilerPath_;  // g++ 编译器路径
    int compileTimeoutMs_;      // 编译阶段超时上限（毫秒，默认 30000）

    QProcess *process_;         // 外部进程对象指针（用于运行 g++ 和学生程序）
    QTimer *timer_;             // 超时计时器指针
    QTemporaryDir *workDir_;    // 本次任务的临时工作目录指针（由 new/delete 管理）
    QElapsedTimer runClock_;    // 用于统计运行耗时的计时器

    Problem problem_;                 // 当前题目副本
    std::vector<TestCase> tests_;     // 本次任务的全部测试点副本（编号 1..testCount）
    int currentIndex_;                // 当前正在运行的测试点下标（从 0 开始）
    int passedCount_;                 // 本次已通过（AC）的测试点数
    int executedCount_;               // 本次确实启动过被测程序的测试点数
    long long totalElapsedMs_;        // 已执行测试点的累计运行耗时（不含编译）
    std::string sourcePath_;          // 学生原始源码路径
    JudgeResult result_;              // 本次判题结果

    bool timedOut_;              // 是否发生超时
    bool outputLimitExceeded_;   // 是否超过输出上限
    bool ioError_;               // 是否发生进程管道读写错误
    std::string ioErrorMessage_; // 进程管道读写错误说明
    std::string cleanupError_;   // 进程树清理异常说明

    int taskId_;       // 任务编号（每次 startJudge 自增，用于隔离旧任务）
    int stepId_;       // 步骤编号（每次启动编译或运行新点时自增）
    int timerStepId_;  // 定时器绑定的步骤编号（防止旧定时器误杀新点）
    bool stepSettled_; // 当前编译或当前点是否已经结算（防止重复结算）

    QByteArray compilerOutput_; // 暂存编译器输出字节
    QByteArray actualOutput_;   // 暂存学生程序标准输出字节
    QByteArray runtimeError_;   // 暂存学生程序错误输出字节

    // 内部辅助函数：统一收尾并发出 judgeFinished 信号
    void finishJudge();

    // 内部辅助函数：编译超时时按本任务有效 PID 调用 taskkill 结束进程树
    void terminateCompileTree();

    // 内部辅助函数：读取进程输出并检查是否超出 16 MiB 上限
    void collectOutput(bool fromStderr);

    // 内部辅助函数：区分工具链组件缺失/异常与正常源码编译错误 CE
    bool isToolchainFailure(const std::string &compilerOutput) const;
};

#endif