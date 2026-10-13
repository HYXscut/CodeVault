#ifndef CODEJUDGE_COMPILER_CHECKER_H
#define CODEJUDGE_COMPILER_CHECKER_H

#include "common/types.h"
#include <QObject>
#include <QProcess>
#include <string>

class QTimer;

class CompilerChecker : public QObject
{
    Q_OBJECT

  public:
    // 构造函数：初始化待检查的编译器路径（不主动发起检查）
    CompilerChecker(std::string compilerPath, QObject *parent = nullptr);

    // 异步发起编译器检查：非阻塞执行，若上一次检查未结束则返回 false
    bool checkCompiler();

    // 获取最近一次完成的检查结果
    CompilerInfo getCompilerInfo();

  signals:
    // 检查完成信号：当次检查结束（成功或失败）后发出
    void checkFinished();

  private slots:
    // 用于启动外部编译器的槽函数
    void startCheck();

    // 当g++.exe运行结束时，由QProcess自动调用的处理函数
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

    // 当g++.exe启动失败或运行出错时，由QProcess自动调用的处理函数
    void onProcessError(QProcess::ProcessError processError);

    // 当超过5s仍未结束时，由QTimer自动调用的超时处理函数
    void onTimeout();

  private:
    std::string compilerPath_; // 待检查的编译器绝对路径
    CompilerInfo info_;        // 用于保存最近一次检查结果的结构体
    QProcess *process_;         // Qt外部进程控制对象指针
    QTimer *timer_;            // Qt5秒超时计时器对象指针
    bool busy_;                // 是否正在进行检查
    bool timeOut_;             // 本次检查是否发生超时
    std::string pendingError_; // 用于暂存进程错误原因的字符串

    // 内部辅助函数：只在进程已停止或未启动时统一写入info_并发出checkFinished信号.
    void finishCheck(bool found, std::string version, std::string error);
};

#endif