// 编译器检查模块：异步检测本地 g++ 是否可用并读取版本号
#ifndef CODEJUDGE_COMPILER_CHECKER_H
#define CODEJUDGE_COMPILER_CHECKER_H

#include "common/types.h"
#include <QObject>
#include <QProcess>
#include <string>

class QTimer;

// 编译器检查类，公有继承自 QObject，用来借助 Qt 的信号槽异步执行 "g++ --version"。
// 主要属性：compilerPath_（编译器路径）、info_（最近一次检查结果）、process_（外部进程）、
// timer_（5 秒超时计时器），以及 busy_、timeOut_、pendingError_ 几个状态标记。
// 对外提供 checkCompiler() 发起检查、getCompilerInfo() 取结果，检查完发出 checkFinished() 信号；
// 内部靠几个私有槽函数和 finishCheck() 配合处理进程结束、出错和超时。
class CompilerChecker : public QObject
{
    Q_OBJECT

  public:
    // 构造函数：保存编译器路径 compilerPath 并初始化进程与计时器，parent 交给 QObject 管理生命周期
    CompilerChecker(std::string compilerPath, QObject *parent = nullptr);

    // 发起一次异步检查（不卡界面）；如果上一次检查还没完直接返回 false，否则返回 true
    bool checkCompiler();

    // 返回最近一次检查得到的结果结构体
    CompilerInfo getCompilerInfo();

  signals:
    // 检查结束信号，不管成功还是失败，每次检查最后都会发一次，通知界面来取结果
    void checkFinished();

  private slots:
    // 真正启动 g++ --version 的槽函数
    void startCheck();

    // 进程结束时由 QProcess 触发，根据退出码 exitCode 和退出状态 exitStatus 判断是否成功
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

    // 进程启动失败或运行异常时由 QProcess 触发，processError 表示具体错误类型
    void onProcessError(QProcess::ProcessError processError);

    // 超过 5 秒还没结束时由 QTimer 触发
    void onTimeout();

  private:
    std::string compilerPath_; // g++ 编译器的完整路径
    CompilerInfo info_;        // 最近一次检查的结果（是否找到、版本号、错误信息）
    QProcess *process_;        // 用来运行外部 g++ 的进程对象
    QTimer *timer_;            // 5 秒超时计时器
    bool busy_;                // 标记当前是否正在检查，防止重复启动
    bool timeOut_;             // 标记这次检查是不是因为超时停掉的
    std::string pendingError_; // 暂存进程出错时的中文说明，方便后续收尾时使用

    // 统一收尾函数：把检查结果（found/version/error）写入 info_，停掉计时器并发出完成信号
    void finishCheck(bool found, std::string version, std::string error);
};

#endif