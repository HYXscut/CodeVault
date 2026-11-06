
#include "judge_engine.h"
#include "output_judge.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimer>
#include <memory>

#include <utility>

JudgeEngine::JudgeEngine(std::string compilerPath, QObject *parent)
    : QObject(parent), compilerPath_(std::move(compilerPath))
{
    process_ = new QProcess(this);
    timer_ = new QTimer(this);

    timer_->setSingleShot(true);

    connect(process_, &QProcess::finished, this, &JudgeEngine::onProcessFinished);

    connect(process_, &QProcess::errorOccurred, this, &JudgeEngine::onProcessError);

    connect(timer_, &QTimer::timeout, this, &JudgeEngine::onTimeout);

    connect(process_, &QProcess::started, this, &JudgeEngine::onProcessStarted);
    connect(process_, &QProcess::readyReadStandardOutput, this, &JudgeEngine::onStdoutReady);
    connect(process_, &QProcess::readyReadStandardError, this, &JudgeEngine::onStderrReady);
}

JudgeEngine::~JudgeEngine() = default;

bool JudgeEngine::isBusy() const
{
    return stage_ != Stage::Idle;
}

JudgeResult JudgeEngine::getResult()
{
    return result_;
}

bool JudgeEngine::startJudge(Problem problem, std::string sourcePath, std::vector<TestCase> tests, std::string &error)
{
    // 1. 防止重复判题
    if (isBusy())
    {
        error = "已有判题任务正在进行";
        return false;
    }

    // 2. 检查题目标识
    if (problem.packageId.empty() || problem.problemId.empty())
    {
        error = "题目标识不能为空";
        return false;
    }

    // 3. 检查时间限制
    if (problem.timeLimitMs <= 0)
    {
        error = "题目时间限制无效";
        return false;
    }

    // 4. 第二周只支持测试点1
    if (tests.size() != 1 || tests[0].index != 1)
    {
        error = "第二周只支持测试点1";
        return false;
    }

    // 5. 检查学生源码
    QString source = QString::fromUtf8(sourcePath.c_str());

    if (!QFileInfo(source).isFile())
    {
        error = "学生源码文件不存在";
        return false;
    }

    // 6. 检查编译器
    QString compiler = QString::fromUtf8(compilerPath_.c_str());

    if (!QFileInfo(compiler).isFile())
    {
        error = "找不到g++编译器";
        return false;
    }

    // 7. 创建 work 父目录
    QString workRoot = QDir(QCoreApplication::applicationDirPath()).filePath("work");

    if (!QDir().mkpath(workRoot))
    {
        error = "无法创建工作目录";
        return false;
    }

    // 8. 为本次判题创建唯一临时目录
    auto tempDir = std::make_unique<QTemporaryDir>(QDir(workRoot).filePath("judge-XXXXXX"));

    if (!tempDir->isValid())
    {
        error = "无法创建临时目录";
        return false;
    }

    // 9. 把学生源码复制为 main.cpp
    QString copiedSource = tempDir->filePath("main.cpp");

    if (!QFile::copy(source, copiedSource))
    {
        error = "复制学生源码失败";
        return false;
    }

    // 保存本次任务的临时工作目录
    workDir_ = std::move(tempDir);

    // 保存题目和测试点
    problem_ = std::move(problem);
    test_ = std::move(tests[0]);
    sourcePath_ = std::move(sourcePath);

    // 清空上一次任务的结果
    result_ = JudgeResult{};

    result_.packageId = problem_.packageId;
    result_.problemId = problem_.problemId;
    result_.sourcePath = sourcePath_;
    result_.testIndex = test_.index;
    result_.expectedOutput = test_.expectedOutput;

    compilerOutput_.clear();
    actualOutput_.clear();
    runtimeError_.clear();

    timedOut_ = false;
    outputLimitExceeded_ = false;
    runClock_.invalidate();
    stage_ = Stage::Compiling;

    error.clear();

    // 等当前调用返回后，再异步启动编译
    QTimer::singleShot(0, this, &JudgeEngine::startCompile);

    return true;
}

void JudgeEngine::startCompile()
{
    if (stage_ != Stage::Compiling || !workDir_)
        return;

    // 设置 g++ 的工作目录
    process_->setWorkingDirectory(workDir_->path());

    // 分开收集 stdout 和 stderr
    process_->setProcessChannelMode(QProcess::SeparateChannels);

    // 为子进程设置工具链 PATH
    QString compilerPath = QString::fromUtf8(compilerPath_.c_str());

    QString compilerBin = QFileInfo(compilerPath).absolutePath();

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    env.insert("PATH", compilerBin + QDir::listSeparator() + env.value("PATH"));

    env.insert("TEMP", workDir_->path());
    env.insert("TMP", workDir_->path());

    process_->setProcessEnvironment(env);

    // g++ 编译参数
    QStringList args;
    args << "-std=c++17";
    args << "main.cpp";
    args << "-o";
    args << "main.exe";

    // 编译最多等待30秒
    timer_->start(30000);

    // 异步启动g++
    process_->start(compilerPath, args);
}

void JudgeEngine::startRun()
{
    // 1. 检查当前状态
    if (stage_ != Stage::Running || !workDir_)
        return;

    // 2. 找到编译生成的 main.exe
    QString exePath = workDir_->filePath("main.exe");

    if (!QFileInfo(exePath).isFile())
    {
        result_.status = "InternalError";
        result_.error = "找不到编译生成的程序";
        finishJudge();
        return;
    }

    // 3. 清空运行阶段的数据
    timedOut_ = false;
    outputLimitExceeded_ = false;
    actualOutput_.clear();
    runtimeError_.clear();

    // 4. 设置工作目录
    process_->setWorkingDirectory(workDir_->path());

    // 5. 分别收集标准输出和错误输出
    process_->setProcessChannelMode(QProcess::SeparateChannels);

    // 6. 异步启动学生程序
    process_->start(exePath);
}

void JudgeEngine::finishJudge()
{
    if (stage_ == Stage::Idle || stage_ == Stage::Cleaning)
        return;

    // 进程未结束，暂时不能清理
    if (process_->state() != QProcess::NotRunning)
        return;

    stage_ = Stage::Cleaning;

    timer_->stop();

    // 清理本次临时工作目录
    if (workDir_)
    {
        if (!workDir_->remove())
        {
            result_.error += "；临时目录清理失败";
        }

        workDir_.reset();
    }

    stage_ = Stage::Idle;

    // 所有结果已经保存，再通知 C
    emit judgeFinished();
}

void JudgeEngine::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // 停止超时计时器
    timer_->stop();

    // 第一种情况：编译器运行结束
    if (stage_ == Stage::Compiling)
    {
        onStdoutReady();
        onStderrReady();

        result_.compilerOutput = compilerOutput_.toStdString();

        QString exePath = workDir_->filePath("main.exe");

        if (timedOut_)
        {
            result_.status = "CE";
            result_.error = "编译超时";
        }
        else if (outputLimitExceeded_)
        {
            result_.status = "CE";
            result_.error = "编译输出超过16 MiB";
        }
        else if (exitStatus != QProcess::NormalExit || exitCode != 0 || !QFileInfo(exePath).isFile())
        {
            result_.status = "CE";
            result_.error = "编译失败";
        }
        else
        {
            // 编译成功，准备运行学生程序
            stage_ = Stage::Running;

            QTimer::singleShot(0, this, &JudgeEngine::startRun);

            return;
        }

        finishJudge();
        return;
    }

    // 第二种情况：学生程序运行结束
    if (stage_ == Stage::Running)
    {
        // 保存实际运行时间
        if (runClock_.isValid())
        {
            result_.elapsedMs = runClock_.elapsed();
        }

        // 读取学生程序的输出
        onStdoutReady();
        onStderrReady();

        result_.actualOutput = actualOutput_.toStdString();

        result_.runtimeError = runtimeError_.toStdString();

        // 优先判断是否超时
        if (timedOut_)
        {
            result_.status = "TLE";
        }
        else if (outputLimitExceeded_)
        {
            result_.status = "RE";
            result_.error = "程序输出超过16 MiB";
        }
        // 再判断是否运行异常
        else if (exitStatus != QProcess::NormalExit || exitCode != 0)
        {
            result_.status = "RE";
        }
        else
        {
            // 程序正常结束，比较输出
            ExactJudge judge;

            bool correct = judge.compare(result_.actualOutput, result_.expectedOutput);

            if (correct)
                result_.status = "AC";
            else
                result_.status = "WA";
        }

        finishJudge();
        return;
    }
}

void JudgeEngine::onProcessError(QProcess::ProcessError error)
{
    // 只处理编译和运行阶段
    if (stage_ != Stage::Compiling && stage_ != Stage::Running)
    {
        return;
    }

    // 处理子进程启动失败
    if (error == QProcess::FailedToStart)
    {
        timer_->stop();

        if (stage_ == Stage::Compiling)
        {
            result_.status = "CE";
            result_.error = "编译器启动失败：";
        }
        else
        {
            result_.status = "RE";
            result_.error = "学生程序启动失败：";
        }

        result_.error += process_->errorString().toUtf8().toStdString();

        if (process_->state() == QProcess::NotRunning)
        {
            finishJudge();
        }

        return;
    }

    // 其他进程错误暂时不在这里结束任务
    // 例如进程崩溃，交给 onProcessFinished() 统一判断
}

void JudgeEngine::onTimeout()
{
    // 只有编译或运行阶段才需要超时处理
    if (stage_ != Stage::Compiling && stage_ != Stage::Running)
    {
        return;
    }

    // 如果进程已经结束，就不需要终止
    if (process_->state() == QProcess::NotRunning)
        return;

    // 标记本次任务发生了超时
    timedOut_ = true;

    // 强制终止当前进程
    process_->kill();

    // 不立即调用 finishJudge()
    // 等 QProcess 发出 finished() 信号后再处理
}

void JudgeEngine::onProcessStarted()
{
    // 只有运行学生程序时才发送测试输入
    if (stage_ != Stage::Running)
        return;

    // 记录程序开始运行的时刻
    runClock_.start();

    // 启动超时倒计时
    timer_->start(problem_.timeLimitMs);

    // 将测试输入转换为 QByteArray
    QByteArray input = QByteArray::fromStdString(test_.input);

    // 把输入发送给学生程序
    process_->write(input);

    // 关闭输入通道，表示输入结束
    process_->closeWriteChannel();
}

void JudgeEngine::onStdoutReady()
{
    collectOutput(false);
}

void JudgeEngine::onStderrReady()
{
    collectOutput(true);
}

void JudgeEngine::collectOutput(bool fromStderr)
{
    if (stage_ != Stage::Compiling && stage_ != Stage::Running)
        return;

    if (outputLimitExceeded_)
        return;

    // 最大保留16 MiB输出
    constexpr qsizetype limit = 16 * 1024 * 1024;

    // 选择要读取的通道
    process_->setReadChannel(fromStderr ? QProcess::StandardError : QProcess::StandardOutput);

    // 确定数据保存在哪个缓冲区
    QByteArray *target = nullptr;
    qsizetype used = 0;

    if (stage_ == Stage::Compiling)
    {
        target = &compilerOutput_;
        used = compilerOutput_.size();
    }
    else
    {
        target = fromStderr ? &runtimeError_ : &actualOutput_;

        used = actualOutput_.size() + runtimeError_.size();
    }

    qsizetype remaining = limit - used;

    // 最多读取剩余容量再加1字节
    QByteArray data = process_->read(remaining + 1);

    if (data.size() > remaining)
    {
        // 只保存限制以内的部分
        target->append(data.left(remaining));

        outputLimitExceeded_ = true;

        if (process_->state() != QProcess::NotRunning)
            process_->kill();

        return;
    }

    target->append(data);
}
