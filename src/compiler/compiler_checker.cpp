// CompilerChecker 类的实现：通过 QProcess 异步检查 g++ 版本并处理超时和异常
#include "compiler/compiler_checker.h"
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QTimer>

using namespace std;

// 构造函数：记录传入的编译器路径 compilerPath，parent 传给基类 QObject。
// 步骤：把状态变量都清成初始值，新建 QProcess 和单次触发的 QTimer，再把进程结束、出错和超时信号连到对应槽函数。
CompilerChecker::CompilerChecker(string compilerPath, QObject *parent) : QObject(parent)
{
    compilerPath_ = compilerPath;
    info_.found = false;
    info_.version = "";
    info_.error = "";

    busy_ = false;
    timeOut_ = false;
    pendingError_ = "";

    process_ = new QProcess(this);
    timer_ = new QTimer(this);
    timer_->setSingleShot(true); // 超时只触发一次，不循环响

    // 绑定外部进程和计时器的回调信号
    connect(process_, &QProcess::finished, this, &CompilerChecker::onProcessFinished);
    connect(process_, &QProcess::errorOccurred, this, &CompilerChecker::onProcessError);
    connect(timer_, &QTimer::timeout, this, &CompilerChecker::onTimeout);
}

// 供主界面调用，发起一次异步检查；正在检查时返回 false，成功接收返回 true。
// 步骤：先确认当前空闲，重置本次检查的状态标记，然后用 singleShot(0) 把真正启动放到下一轮事件循环，
// 这样函数能立刻返回，不会卡住界面。
bool CompilerChecker::checkCompiler()
{
    if (busy_)
    {
        return false;
    }

    busy_ = true;
    timeOut_ = false;
    pendingError_ = "";
    QTimer::singleShot(0, this, &CompilerChecker::startCheck);
    return true;
}

// 返回最近一次检查完成的 CompilerInfo 结果副本。
CompilerInfo CompilerChecker::getCompilerInfo()
{
    return info_;
}

// 统一收尾函数：found 表示是否找到可用编译器，version 存版本号，error 存失败原因。
// 步骤：如果已经不在检查状态就直接返回（避免出错和超时撞在一起发两次信号）；
// 否则停掉计时器、写入 info_、解除忙碌状态，最后发出 checkFinished() 通知界面。
void CompilerChecker::finishCheck(bool found, string version, string error)
{
    if (!busy_)
    {
        return;
    }
    timer_->stop();
    info_.found = found;
    info_.version = version;
    info_.error = error;
    busy_ = false;

    emit checkFinished();
}

// 真正启动外部 g++ 进程执行 "--version"。
// 步骤：先把路径转成 QString 检查文件在不在，清掉进程缓冲区里可能残留的旧输出，
// 开 5 秒超时计时器，再带着 "--version" 参数异步启动编译器。
void CompilerChecker::startCheck()
{
    if (!busy_)
    {
        return;
    }

    // 先确认编译器文件存在，找不到就直接报错收尾
    QString programPath = QString::fromUtf8(compilerPath_.c_str());
    QFileInfo fileInfo(programPath);
    if (compilerPath_.empty() || !fileInfo.exists() || !fileInfo.isFile())
    {
        finishCheck(false, "", "找不到指定编译器");
        return;
    }

    // 清掉上次可能残留的缓冲输出，开 5 秒定时并启动 g++ --version
    process_->readAllStandardOutput();
    process_->readAllStandardError();

    timer_->start(5000);

    QStringList arguments;
    arguments.push_back("--version");
    process_->start(programPath, arguments);
}

// g++ 进程退出后由 QProcess 自动调用；exitCode 是退出码，exitStatus 表示正常退出还是崩溃。
// 步骤：先检查是不是超时或前面已记录的错误引起的退出（防止 kill 后被当成普通失败覆盖），
// 没问题再读 stdout 和 stderr；只有正常退出、退出码为 0 且输出非空才算检查成功。
void CompilerChecker::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (!busy_)
    {
        return;
    }

    // 超时后主动 kill 也会进这里，优先按超时处理
    if (timeOut_)
    {
        finishCheck(false, "", "编译器检查超时");
        return;
    }

    // 前面 onProcessError 记了错并强杀进程时，保留原来的出错原因
    if (!pendingError_.empty())
    {
        finishCheck(false, "", pendingError_);
        return;
    }

    // 取出标准输出和错误输出，顺便去掉首尾空白
    string outText = QString::fromUtf8(process_->readAllStandardOutput()).trimmed().toStdString();
    string errText = QString::fromUtf8(process_->readAllStandardError()).trimmed().toStdString();

    // 正常退出、退出码为 0 且读到了版本文字，才判定编译器可用
    if (exitStatus == QProcess::NormalExit && exitCode == 0 && !outText.empty())
    {
        finishCheck(true, outText, "");
    }
    else
    {
        if (!errText.empty())
        {
            finishCheck(false, "", errText);
        }
        else
        {
            finishCheck(false, "", "编译器未正常返回版本信息");
        }
    }
}

// g++ 启动失败或运行中出错时由 QProcess 自动调用，processError 是错误类型。
// 步骤：如果已经超时，按超时收尾；否则区分“启动失败”和“运行异常”记下中文提示。
// 进程如果根本没跑起来（NotRunning），不会再触发 finished 信号，这里直接收尾；
// 如果还在运行，就强杀掉，交给 onProcessFinished 统一收尾。
void CompilerChecker::onProcessError(QProcess::ProcessError processError)
{
    if (!busy_)
    {
        return;
    }

    // 已经超时的话按超时处理，不用再覆盖错误原因
    if (timeOut_)
    {
        if (process_->state() == QProcess::NotRunning)
        {
            finishCheck(false, "", "编译器检查超时");
        }
        return;
    }

    // 启动失败和运行中途出错分开写提示，方便排查
    if (processError == QProcess::FailedToStart)
    {
        pendingError_ = "编译器启动失败，请检查文件权限或依赖是否完整";
    }
    else
    {
        pendingError_ = "编译器运行过程中发生异常";
    }

    // 没跑起来就不会触发 finished，直接收尾；还在跑就 kill 掉等 onProcessFinished 处理
    if (process_->state() == QProcess::NotRunning)
    {
        finishCheck(false, "", pendingError_);
    }
    else
    {
        process_->kill();
    }
}

// 超过 5 秒 g++ 还没结束时由 QTimer 自动调用。
// 步骤：把 timeOut_ 设为 true，进程已停就直接收尾，还在跑就 kill 掉，等它退出后由 onProcessFinished 收尾。
void CompilerChecker::onTimeout()
{
    if (!busy_)
    {
        return;
    }
    timeOut_ = true;

    if (process_->state() == QProcess::NotRunning)
    {
        finishCheck(false, "", "编译器检查超时");
    }
    else
    {
        process_->kill();
    }
}