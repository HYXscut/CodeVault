#include "compiler/compiler_checker.h"
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QTimer>

using namespace std;

// CompilerChecker类构造函数，初始化成员变量，创建QProcess和QTimer实例并链接信号与槽
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
    timer_->setSingleShot(true); // 到点只响一次

    // 链接信号与槽，依次为信号发出者、发出的信号内容、信号接收者、信号处理函数
    connect(process_, &QProcess::finished, this, &CompilerChecker::onProcessFinished);
    connect(process_, &QProcess::errorOccurred, this, &CompilerChecker::onProcessError);
    connect(timer_, &QTimer::timeout, this, &CompilerChecker::onTimeout);
}

// 由成员C调用，以检查编译器
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

// 获取最近一次完成的编译器检查结果
CompilerInfo CompilerChecker::getCompilerInfo()
{
    return info_;
}

// 收尾函数：停止计时器，保存结果，重置状态并发出完成信号
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

    // 发出信号通知C，检查已完成，可调用getCompilerInfo()查看检查结果
    emit checkFinished();
}

// 启动外部编译器
void CompilerChecker::startCheck()
{
    if (!busy_)
    {
        return;
    }

    // string转QString，并检查编译器是否存在
    QString programPath = QString::fromUtf8(compilerPath_.c_str());
    QFileInfo fileInfo(programPath);
    if (compilerPath_.empty() || !fileInfo.exists() || !fileInfo.isFile())
    {
        finishCheck(false, "", "找不到指定编译器");
        return;
    }

    // 清空可能存在的残留输出
    process_->readAllStandardOutput();
    process_->readAllStandardError();

    // 启动五秒超时计时器
    timer_->start(5000);

    // 启动外部编译器
    QStringList arguments;
    arguments.push_back("--version");
    process_->start(programPath, arguments);
}

// 当编译器检查结束时由QProcess自动调用
void CompilerChecker::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (!busy_)
    {
        return;
    }

    //若超时，优先按超时报错处理
    if (timeOut_)
    {
        finishCheck(false, "", "编译器检查超时");
        return;
    }

    // 若之前记录了运行错误，只是因为被系统强杀而交给onProcessFinished()处理，按原错误处理
    if (!pendingError_.empty())
    {
        finishCheck(false, "", pendingError_);
        return;
    }

    //读取标准输出和标准错误
    string outText = QString::fromUtf8(process_->readAllStandardOutput()).trimmed().toStdString();
    string errText = QString::fromUtf8(process_->readAllStandardError()).trimmed().toStdString();

    //只有当正常退出，退出码为0，输出不为空才判断为成功
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


//当外部进程启动失败或程序运行出错时，QProcess自动调用
void CompilerChecker::onProcessError(QProcess::ProcessError processError)
{
    if (!busy_)
    {
        return;
    }

    // 若超时，优先按超时报错处理
    if (timeOut_)
    {
        if (process_->state() == QProcess::NotRunning)
        {
            finishCheck(false, "", "编译器检查超时");
        }
        return;
    }
    //启动没成功和运行过程报错分开写错误原因
    if (processError == QProcess::FailedToStart)
    {
        pendingError_ = "编译器启动失败，请检查文件权限或依赖是否完整";
    }
    else
    {
        pendingError_ = "编译器运行过程中发生异常";
    }
    // 如果进程已经停了或没启动成功，直接收尾通知；否则先强制杀死，让后面的 onProcessFinished 收尾
    if (process_->state() == QProcess::NotRunning)
    {
        finishCheck(false, "", pendingError_);
    }
    else
    {
        process_->kill();
    }
}

// 若超过五秒倒计时仍未结束，由QTimer自动调用
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