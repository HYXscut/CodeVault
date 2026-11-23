
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

using namespace std;

JudgeEngine::JudgeEngine(string compilerPath, QObject *parent)
    : QObject(parent)
{
    // 在构造函数体内用普通赋值初始化成员变量（与 CompilerChecker 写法一致）
    compilerPath_ = compilerPath;
    compileTimeoutMs_ = 30000;
    stage_ = STAGE_IDLE;
    workDir_ = nullptr;
    currentIndex_ = 0;
    passedCount_ = 0;
    executedCount_ = 0;
    totalElapsedMs_ = 0;
    timedOut_ = false;
    outputLimitExceeded_ = false;
    ioError_ = false;
    ioErrorMessage_.clear();
    cleanupError_.clear();
    taskId_ = 0;
    stepId_ = 0;
    timerStepId_ = 0;
    stepSettled_ = false;

    process_ = new QProcess(this);
    timer_ = new QTimer(this);
    timer_->setSingleShot(true); // 超时定时器只触发一次

    connect(process_, &QProcess::finished, this, &JudgeEngine::onProcessFinished);
    connect(process_, &QProcess::errorOccurred, this, &JudgeEngine::onProcessError);
    connect(timer_, &QTimer::timeout, this, &JudgeEngine::onTimeout);
    connect(process_, &QProcess::started, this, &JudgeEngine::onProcessStarted);
    connect(process_, &QProcess::readyReadStandardOutput, this, &JudgeEngine::onStdoutReady);
    connect(process_, &QProcess::readyReadStandardError, this, &JudgeEngine::onStderrReady);
}

JudgeEngine::~JudgeEngine()
{
    // 若对象销毁时仍有外部进程在运行，先终止并等待退出，避免目录被占用
    if (process_ != nullptr && process_->state() != QProcess::NotRunning)
    {
        process_->kill();
        process_->waitForFinished(1000);
    }

    // 如果对象销毁时临时目录指针还没释放，在这里手动 delete，防止内存泄漏
    if (workDir_ != nullptr)
    {
        delete workDir_;
        workDir_ = nullptr;
    }
}

bool JudgeEngine::isBusy() const
{
    return stage_ != STAGE_IDLE;
}

void JudgeEngine::setCompileTimeoutMs(int timeoutMs)
{
    if (timeoutMs > 0)
    {
        compileTimeoutMs_ = timeoutMs;
    }
}

JudgeResult JudgeEngine::getResult()
{
    return result_;
}

bool JudgeEngine::startJudge(Problem problem, string sourcePath, vector<TestCase> tests, string &error)
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

    // 4. 第三周：检查测试点数量与连续编号（1..testCount）
    if (problem.testCount <= 0 || tests.empty())
    {
        error = "题目测试点数量必须大于 0";
        return false;
    }

    if ((int)tests.size() != problem.testCount)
    {
        error = "传入的测试点数量与题目 testCount 不一致";
        return false;
    }

    for (int i = 0; i < (int)tests.size(); ++i)
    {
        if (tests[i].index != i + 1)
        {
            error = "测试点编号必须从 1 开始连续递增";
            return false;
        }
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

    // 7. 创建并检查 work 父目录是否可写
    QString workRoot = QDir(QCoreApplication::applicationDirPath()).filePath("work");

    if (!QDir().mkpath(workRoot))
    {
        error = "无法创建或写入工作目录，请将软件放到普通用户可写的英文目录";
        return false;
    }

    // 8. 为本次判题创建唯一临时目录（用普通指针 + new 创建，同时校验目录可写性）
    QTemporaryDir *tempDir = new QTemporaryDir(QDir(workRoot).filePath("judge-XXXXXX"));

    if (!tempDir->isValid())
    {
        QString reason = tempDir->errorString();
        delete tempDir; // 创建失败也要及时释放内存，防止泄漏
        error = "工作目录不可写或无法创建临时目录";
        if (!reason.isEmpty())
        {
            error += "（" + reason.toUtf8().toStdString() + "）";
        }
        error += "，请将软件放到普通用户可写的英文目录";
        return false;
    }

    // 9. 把学生源码复制为 main.cpp
    QString copiedSource = tempDir->filePath("main.cpp");

    if (!QFile::copy(source, copiedSource))
    {
        delete tempDir; // 复制失败提前返回前释放 tempDir
        error = "复制学生源码失败";
        return false;
    }

    // 保存本次任务的临时工作目录
    if (workDir_ != nullptr)
    {
        delete workDir_;
        workDir_ = nullptr;
    }
    workDir_ = tempDir;

    // 保存题目、全部测试点和原源码路径副本
    problem_ = problem;
    tests_ = tests;
    sourcePath_ = sourcePath;
    currentIndex_ = 0;
    passedCount_ = 0;
    executedCount_ = 0;
    totalElapsedMs_ = 0;

    // 清空上一次任务的结果（全部 AC 或 CE 时 testIndex 约定为 0）
    result_ = JudgeResult();

    result_.packageId = problem_.packageId;
    result_.problemId = problem_.problemId;
    result_.sourcePath = sourcePath_;
    result_.testIndex = 0;

    compilerOutput_.clear();
    actualOutput_.clear();
    runtimeError_.clear();

    timedOut_ = false;
    outputLimitExceeded_ = false;
    ioError_ = false;
    ioErrorMessage_.clear();
    cleanupError_.clear();
    ++taskId_;
    ++stepId_;
    timerStepId_ = 0;
    stepSettled_ = false;
    runClock_.invalidate();
    stage_ = STAGE_COMPILING;

    error.clear();

    // 等当前调用返回后，再异步启动编译
    QTimer::singleShot(0, this, &JudgeEngine::startCompile);

    return true;
}

void JudgeEngine::startCompile()
{
    if (stage_ != STAGE_COMPILING || workDir_ == nullptr)
        return;

    ++stepId_;
    stepSettled_ = false;

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

    // 记录本次定时器所属步骤编号，默认编译最多等待 30 秒（可通过 setCompileTimeoutMs 调整）
    timerStepId_ = stepId_;
    timer_->start(compileTimeoutMs_);

    // 异步启动g++
    process_->start(compilerPath, args);
}

void JudgeEngine::startRun()
{
    // 1. 检查当前状态与当前点下标范围
    if (stage_ != STAGE_RUNNING || workDir_ == nullptr)
        return;

    ++stepId_;
    stepSettled_ = false;

    if (currentIndex_ < 0 || currentIndex_ >= (int)tests_.size())
    {
        stepSettled_ = true;
        result_.status = "InternalError";
        result_.error = "测试点下标越界";
        finishJudge();
        return;
    }

    // 2. 找到编译生成的 main.exe
    QString exePath = workDir_->filePath("main.exe");

    if (!QFileInfo(exePath).isFile())
    {
        stepSettled_ = true;
        result_.status = "InternalError";
        result_.error = "找不到编译生成的程序";
        finishJudge();
        return;
    }

    // 3. 重置当前点运行阶段的标志与缓冲
    timedOut_ = false;
    outputLimitExceeded_ = false;
    ioError_ = false;
    ioErrorMessage_.clear();
    actualOutput_.clear();
    runtimeError_.clear();
    runClock_.invalidate();

    // 4. 设置工作目录，并把工具链 bin 目录放到 PATH 最前面，
    //    确保在无系统 g++ 的电脑上运行 main.exe 时也能找到配套运行时 DLL
    process_->setWorkingDirectory(workDir_->path());

    QString compilerPath = QString::fromUtf8(compilerPath_.c_str());
    QString compilerBin = QFileInfo(compilerPath).absolutePath();
    QProcessEnvironment runEnv = QProcessEnvironment::systemEnvironment();
    runEnv.insert("PATH", compilerBin + QDir::listSeparator() + runEnv.value("PATH"));
    process_->setProcessEnvironment(runEnv);

    // 5. 分别收集标准输出和错误输出
    process_->setProcessChannelMode(QProcess::SeparateChannels);

    // 6. 异步启动学生程序运行当前测试点
    process_->start(exePath);
}

void JudgeEngine::finishJudge()
{
    if (stage_ == STAGE_IDLE || stage_ == STAGE_CLEANING)
        return;

    // 如果外部进程仍在运行，先短等其结束；仍未退出则保持引擎锁定并报清理错误
    if (process_->state() != QProcess::NotRunning)
    {
        process_->waitForFinished(1000);
        if (process_->state() != QProcess::NotRunning)
        {
            result_.status = "InternalError";
            if (!result_.error.empty())
                result_.error += "；";
            result_.error += "外部进程未能退出，判题引擎保持锁定";
            return;
        }
    }

    stage_ = STAGE_CLEANING;

    timer_->stop();

    // 若进程树清理阶段记录了错误，统一按系统错误处理
    if (!cleanupError_.empty())
    {
        result_.status = "InternalError";
        if (!result_.error.empty())
            result_.error += "；";
        result_.error += cleanupError_;
    }

    // 清理本次临时工作目录并释放指针（目录清理失败按系统错误处理，但不阻止后续使用新目录）
    if (workDir_ != nullptr)
    {
        if (!workDir_->remove())
        {
            result_.status = "InternalError";
            if (!result_.error.empty())
                result_.error += "；";
            result_.error += "临时目录清理失败";
        }

        delete workDir_;
        workDir_ = nullptr;
    }

    tests_.clear();
    stage_ = STAGE_IDLE;

    // 所有结果已经保存，再通知 C
    emit judgeFinished();
}

void JudgeEngine::terminateCompileTree()
{
    // 在父进程还未被杀死前获取其有效 PID，避免 PID 失效后找不到子进程
    long long pid = (long long)process_->processId();
    bool taskkillOk = false;
    string killDetail;

    if (pid > 0)
    {
        QProcess killer;
        QStringList killArgs;
        killArgs << "/PID" << QString::number(pid) << "/T" << "/F";
        killer.start("taskkill", killArgs);

        if (killer.waitForFinished(5000))
        {
            if (killer.exitStatus() == QProcess::NormalExit && killer.exitCode() == 0)
            {
                taskkillOk = true;
            }
            else
            {
                string errText = QString::fromLocal8Bit(killer.readAllStandardError()).trimmed().toStdString();
                string outText = QString::fromLocal8Bit(killer.readAllStandardOutput()).trimmed().toStdString();
                killDetail = "exitCode=" + to_string(killer.exitCode());
                if (!errText.empty())
                    killDetail += ", " + errText;
                else if (!outText.empty())
                    killDetail += ", " + outText;
            }
        }
        else
        {
            killDetail = "taskkill执行超时或启动失败: " + killer.errorString().toUtf8().toStdString();
            if (killer.state() != QProcess::NotRunning)
            {
                killer.kill();
                killer.waitForFinished(500);
            }
        }
    }
    else
    {
        killDetail = "无法获取编译器PID";
    }

    // 如果 taskkill 未成功且编译器仍在运行，记下清理错误并兜底调用 kill()
    if (!taskkillOk && process_->state() != QProcess::NotRunning)
    {
        cleanupError_ = "编译超时，清理编译器进程树失败";
        if (!killDetail.empty())
            cleanupError_ += "（" + killDetail + "）";
        process_->kill();
    }
}

void JudgeEngine::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (stage_ != STAGE_COMPILING && stage_ != STAGE_RUNNING)
        return;

    // 防止同一步骤重复结算
    if (stepSettled_)
        return;
    stepSettled_ = true;

    // 停止超时计时器
    timer_->stop();

    // 第一种情况：编译器运行结束
    if (stage_ == STAGE_COMPILING)
    {
        onStdoutReady();
        onStderrReady();

        result_.compilerOutput = compilerOutput_.toStdString();

        QString exePath = workDir_->filePath("main.exe");

        if (timedOut_)
        {
            result_.status = "InternalError";
            if (compileTimeoutMs_ >= 1000 && compileTimeoutMs_ % 1000 == 0)
            {
                result_.error = "编译超时（超过 " + to_string(compileTimeoutMs_ / 1000) + " 秒）";
            }
            else
            {
                result_.error = "编译超时（超过 " + to_string(compileTimeoutMs_) + " 毫秒）";
            }
        }
        else if (outputLimitExceeded_)
        {
            result_.status = "InternalError";
            result_.error = "编译输出超过 16 MiB 上限";
        }
        else if (ioError_)
        {
            result_.status = "InternalError";
            result_.error = ioErrorMessage_;
        }
        else if (exitStatus != QProcess::NormalExit)
        {
            result_.status = "InternalError";
            result_.error = "编译器异常崩溃退出";
        }
        else if (exitCode != 0)
        {
            if (isToolchainFailure(result_.compilerOutput))
            {
                result_.status = "InternalError";
                result_.error = "工具链运行异常或组件缺失，请检查编译器目录是否完整";
            }
            else
            {
                // 只有编译器正常退出、返回非 0 退出码且非工具链故障时，才判定为源码编译错误 CE
                result_.status = "CE";
                result_.error = "编译失败";
            }
        }
        else if (!QFileInfo(exePath).isFile())
        {
            result_.status = "InternalError";
            result_.error = "编译器未生成可执行文件 main.exe";
        }
        else
        {
            // 编译成功（整次任务只编译这一次），从第 0 号下标（第 1 个测试点）开始逐点运行
            currentIndex_ = 0;
            stage_ = STAGE_RUNNING;

            QTimer::singleShot(0, this, &JudgeEngine::startRun);

            return;
        }

        finishJudge();
        return;
    }

    // 第二种情况：学生程序当前测试点运行结束
    if (stage_ == STAGE_RUNNING)
    {
        // 计算当前测试点的运行耗时并累加到总耗时
        long long pointElapsedMs = 0;
        if (runClock_.isValid())
        {
            pointElapsedMs = runClock_.elapsed();
        }
        totalElapsedMs_ += pointElapsedMs;

        // 读取学生程序在当前测试点残留的输出
        onStdoutReady();
        onStderrReady();

        string pointActual = actualOutput_.toStdString();
        string pointExpected = tests_[currentIndex_].expectedOutput;
        string pointStderr = runtimeError_.toStdString();
        string pointStatus;

        // 1. 先检查是否已标记超时（防止 kill() 造成的非零退出覆盖 TLE）
        if (timedOut_)
        {
            pointStatus = "TLE";
            if (!pointStderr.empty())
                pointStderr += "\n";
            pointStderr += "程序运行超时（时限 " + to_string(problem_.timeLimitMs) + " ms）";
        }
        // 2. 再检查是否输出超限（第三周独立状态 OutputLimit，不能用截断输出判 AC/WA）
        else if (outputLimitExceeded_)
        {
            pointStatus = "OutputLimit";
            result_.error = "程序输出超过 16 MiB 上限";
        }
        // 3. 再检查是否发生管道读写错误（系统错误，不能用不完整输出判 AC/WA）
        else if (ioError_)
        {
            pointStatus = "InternalError";
            result_.error = ioErrorMessage_;
        }
        // 4. 再检查是否异常崩溃或非零退出（RE）
        else if (exitStatus != QProcess::NormalExit || exitCode != 0)
        {
            pointStatus = "RE";
            if (!pointStderr.empty())
                pointStderr += "\n";
            if (exitStatus != QProcess::NormalExit)
                pointStderr += "程序异常崩溃退出";
            else
                pointStderr += "程序非零退出（退出码：" + to_string(exitCode) + "）";
        }
        // 5. 正常退出且未触发保护，调用多态比较器判断 AC / WA
        else
        {
            ExactJudge exactJudge;
            OutputJudge *judge = &exactJudge;

            bool correct = judge->compare(pointActual, pointExpected);

            if (correct)
                pointStatus = "AC";
            else
                pointStatus = "WA";
        }

        // 如果当前点没有通过，立即停止后续测试点并收尾
        if (pointStatus != "AC")
        {
            result_.status = pointStatus;
            result_.testIndex = tests_[currentIndex_].index;
            result_.elapsedMs = totalElapsedMs_;
            result_.actualOutput = pointActual;
            result_.expectedOutput = pointExpected;
            result_.runtimeError = pointStderr;

            finishJudge();
            return;
        }

        // 当前点 AC，通过数加一
        ++passedCount_;

        // 如果后面还有测试点，就继续异步启动下一个点
        if (currentIndex_ + 1 < (int)tests_.size())
        {
            ++currentIndex_;
            QTimer::singleShot(0, this, &JudgeEngine::startRun);
            return;
        }

        // 所有测试点均已通过：testIndex 为 0，过渡输出字段保留最后一个点
        result_.status = "AC";
        result_.testIndex = 0;
        result_.elapsedMs = totalElapsedMs_;
        result_.actualOutput = pointActual;
        result_.expectedOutput = pointExpected;
        result_.runtimeError = pointStderr;

        finishJudge();
        return;
    }
}

void JudgeEngine::onProcessError(QProcess::ProcessError error)
{
    // 只处理编译和运行阶段，且当前步骤尚未结算
    if (stage_ != STAGE_COMPILING && stage_ != STAGE_RUNNING)
    {
        return;
    }
    if (stepSettled_)
    {
        return;
    }

    // 处理子进程启动失败（第三周：无论是编译器还是被测程序启动失败，均归为 InternalError）
    if (error == QProcess::FailedToStart)
    {
        stepSettled_ = true;
        timer_->stop();

        result_.status = "InternalError";
        if (stage_ == STAGE_COMPILING)
        {
            result_.error = "编译器启动失败：";
        }
        else
        {
            result_.testIndex = tests_[currentIndex_].index;
            result_.elapsedMs = totalElapsedMs_;
            result_.expectedOutput = tests_[currentIndex_].expectedOutput;
            result_.error = "学生程序启动失败：";
        }

        result_.error += process_->errorString().toUtf8().toStdString();

        if (process_->state() == QProcess::NotRunning)
        {
            finishJudge();
        }

        return;
    }

    // 处理进程通信读写错误（不能拿不完整输出去判 AC/WA）
    if (error == QProcess::ReadError || error == QProcess::WriteError)
    {
        ioError_ = true;
        ioErrorMessage_ = "进程通信读写失败：" + process_->errorString().toUtf8().toStdString();
        if (process_->state() != QProcess::NotRunning)
        {
            process_->kill();
        }
        return;
    }

    // 其他进程错误（例如 Crashed）交给 onProcessFinished() 统一判断
}

void JudgeEngine::onTimeout()
{
    // 只有编译或运行阶段才需要超时处理
    if (stage_ != STAGE_COMPILING && stage_ != STAGE_RUNNING)
    {
        return;
    }

    // 核对步骤编号和结算标志，防止旧点的迟到定时器误伤新点
    if (stepSettled_ || timerStepId_ != stepId_)
    {
        return;
    }

    // 如果进程已经结束，就不需要终止
    if (process_->state() == QProcess::NotRunning)
        return;

    // 标记本次步骤发生了超时
    timedOut_ = true;

    if (stage_ == STAGE_COMPILING)
    {
        // 编译超时可能存在配套子进程，按本任务有效 PID 清理整棵进程树
        terminateCompileTree();
    }
    else
    {
        // 学生单文件程序直接强制终止
        process_->kill();
    }

    // 不立即调用 finishJudge()
    // 等 QProcess 发出 finished() 信号后再统一收尾
}

void JudgeEngine::onProcessStarted()
{
    // 只有运行学生程序且当前点尚未结算时才发送输入
    if (stage_ != STAGE_RUNNING || stepSettled_)
        return;

    // 确认被测程序已成功启动，已执行数加一
    ++executedCount_;

    // 记录当前点开始运行的时刻
    runClock_.start();

    // 绑定步骤编号并启动当前点的超时倒计时
    timerStepId_ = stepId_;
    timer_->start(problem_.timeLimitMs);

    // 将当前测试点的输入转换为 QByteArray 并发送给学生程序
    QByteArray input = QByteArray::fromStdString(tests_[currentIndex_].input);

    process_->write(input);

    // 关闭输入通道，表示输入结束（让等待 EOF 的程序能正常结束）
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
    if (stage_ != STAGE_COMPILING && stage_ != STAGE_RUNNING)
        return;

    if (outputLimitExceeded_)
        return;

    // 最大保留 16 MiB 输出（用普通 long long 常量表示字节数）
    const long long limit = 16 * 1024 * 1024;

    // 用普通 if/else 选择要读取的通道
    if (fromStderr)
    {
        process_->setReadChannel(QProcess::StandardError);
    }
    else
    {
        process_->setReadChannel(QProcess::StandardOutput);
    }

    // 确定数据保存在哪个缓冲区，并统计已读字节数
    QByteArray *target = nullptr;
    long long used = 0;

    if (stage_ == STAGE_COMPILING)
    {
        target = &compilerOutput_;
        used = compilerOutput_.size();
    }
    else
    {
        if (fromStderr)
        {
            target = &runtimeError_;
        }
        else
        {
            target = &actualOutput_;
        }

        used = actualOutput_.size() + runtimeError_.size();
    }

    long long remaining = limit - used;

    // 最多读取剩余容量再加 1 字节，用来判断是否超限
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

bool JudgeEngine::isToolchainFailure(const string &compilerOutput) const
{
    // 1. 先检查编译器输出是否全为空白：正常源码编译错误一定会有诊断文字；
    // 若非零退出却没有任何输出，通常是缺 DLL 或系统环境异常。
    bool hasVisibleChar = false;
    for (size_t i = 0; i < compilerOutput.size(); ++i)
    {
        char ch = compilerOutput[i];
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
        {
            hasVisibleChar = true;
            break;
        }
    }
    if (!hasVisibleChar)
    {
        return true;
    }

    // 2. 检查是否包含工具链内部子程序缺失、启动失败或编译器崩溃的典型字样
    const char *patterns[] = {
        "cannot execute",
        "cannot exec",
        "CreateProcess",
        "installation problem",
        "error trying to exec",
        "cannot find 'ld'",
        "cannot find 'as'",
        "cannot find 'cc1plus'",
        "internal compiler error"
    };
    const int patternCount = (int)(sizeof(patterns) / sizeof(patterns[0]));

    for (int i = 0; i < patternCount; ++i)
    {
        if (compilerOutput.find(patterns[i]) != string::npos)
        {
            return true;
        }
    }

    return false;
}
