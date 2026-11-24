// JudgeEngine 类的实现：一次编译、逐个运行测试点、首错即停、超时与输出上限控制
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

// 构造函数：记下编译器路径 compilerPath，parent 传给基类 QObject。
// 步骤：把各个状态标志和计数器都初始化一遍，默认编译超时设成 30 秒，
// 创建 QProcess 和单次触发的 QTimer，并把进程和计时器的 6 个信号都连到对应槽函数。
JudgeEngine::JudgeEngine(string compilerPath, QObject *parent)
    : QObject(parent)
{
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
    timer_->setSingleShot(true); // 计时器每次到点只响一次

    // 绑定子进程和超时计时器的全部事件回调
    connect(process_, &QProcess::finished, this, &JudgeEngine::onProcessFinished);
    connect(process_, &QProcess::errorOccurred, this, &JudgeEngine::onProcessError);
    connect(timer_, &QTimer::timeout, this, &JudgeEngine::onTimeout);
    connect(process_, &QProcess::started, this, &JudgeEngine::onProcessStarted);
    connect(process_, &QProcess::readyReadStandardOutput, this, &JudgeEngine::onStdoutReady);
    connect(process_, &QProcess::readyReadStandardError, this, &JudgeEngine::onStderrReady);
}

// 析构函数：对象销毁前做清理，防止留下孤儿进程或临时目录泄漏。
// 步骤：如果外部进程还在跑，先强杀并最多等 1 秒让它退出（不然临时目录会被占用删不掉），
// 再释放 workDir_ 指向的临时目录对象。
JudgeEngine::~JudgeEngine()
{
    if (process_ != nullptr && process_->state() != QProcess::NotRunning)
    {
        process_->kill();
        process_->waitForFinished(1000);
    }

    if (workDir_ != nullptr)
    {
        delete workDir_;
        workDir_ = nullptr;
    }
}

// 返回引擎当前是否正在执行任务：只要不是空闲阶段就说明还在忙。
bool JudgeEngine::isBusy() const
{
    return stage_ != STAGE_IDLE;
}

// 调整编译超时时间 timeoutMs（毫秒），只接受正数，防止误传 0 或负数。
void JudgeEngine::setCompileTimeoutMs(int timeoutMs)
{
    if (timeoutMs > 0)
    {
        compileTimeoutMs_ = timeoutMs;
    }
}

// 返回最近一次判题保存好的 JudgeResult 副本。
JudgeResult JudgeEngine::getResult()
{
    return result_;
}

// 接收并启动一次新的异步判题任务（由界面调用）。
// 参数：problem 是题目信息，sourcePath 是学生源码路径，tests 是按 1..testCount 排好的测试点，
// error 用来传出拒绝原因。接收成功返回 true，任何前置检查不通过则返回 false。
// 步骤：先逐项校验忙碌状态、题目字段、测试点编号、源码和编译器文件；再在程序目录的 work/ 下
// 新建英文临时目录并把源码复制成 main.cpp；最后重置本次任务状态并异步触发 startCompile()。
bool JudgeEngine::startJudge(Problem problem, string sourcePath, vector<TestCase> tests, string &error)
{
    // 正在判题时拒绝新任务
    if (isBusy())
    {
        error = "已有判题任务正在进行";
        return false;
    }

    // 核对题目标识、时限和测试点数量，测试点编号要求从 1 开始连续递增
    if (problem.packageId.empty() || problem.problemId.empty())
    {
        error = "题目标识不能为空";
        return false;
    }

    if (problem.timeLimitMs <= 0)
    {
        error = "题目时间限制无效";
        return false;
    }

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

    // 确认学生源码和 g++ 编译器文件都存在
    QString source = QString::fromUtf8(sourcePath.c_str());

    if (!QFileInfo(source).isFile())
    {
        error = "学生源码文件不存在";
        return false;
    }

    QString compiler = QString::fromUtf8(compilerPath_.c_str());

    if (!QFileInfo(compiler).isFile())
    {
        error = "找不到g++编译器";
        return false;
    }

    // 在程序目录下的 work/ 里建本次专用的英文临时目录，顺便检验目录是否可写
    QString workRoot = QDir(QCoreApplication::applicationDirPath()).filePath("work");

    if (!QDir().mkpath(workRoot))
    {
        error = "无法创建或写入工作目录，请将软件放到普通用户可写的英文目录";
        return false;
    }

    QTemporaryDir *tempDir = new QTemporaryDir(QDir(workRoot).filePath("judge-XXXXXX"));

    if (!tempDir->isValid())
    {
        QString reason = tempDir->errorString();
        delete tempDir; // 创建失败直接释放对象，避免内存泄漏
        error = "工作目录不可写或无法创建临时目录";
        if (!reason.isEmpty())
        {
            error += "（" + reason.toUtf8().toStdString() + "）";
        }
        error += "，请将软件放到普通用户可写的英文目录";
        return false;
    }

    // 把学生源码复制到临时目录改名 main.cpp，既避开中文或空格路径，也不弄脏原文件夹
    QString copiedSource = tempDir->filePath("main.cpp");

    if (!QFile::copy(source, copiedSource))
    {
        delete tempDir; // 复制失败提前返回前释放临时目录对象
        error = "复制学生源码失败";
        return false;
    }

    // 切换新临时目录并保存本次任务用到的题目、测试点和原路径
    if (workDir_ != nullptr)
    {
        delete workDir_;
        workDir_ = nullptr;
    }
    workDir_ = tempDir;

    problem_ = problem;
    tests_ = tests;
    sourcePath_ = sourcePath;
    currentIndex_ = 0;
    passedCount_ = 0;
    executedCount_ = 0;
    totalElapsedMs_ = 0;

    // 清空上次任务留下的结果、输出缓冲和状态标记（全 AC 或 CE 时 testIndex 记 0）
    result_ = JudgeResult();

    result_.packageId = problem_.packageId;
    result_.problemId = problem_.problemId;
    result_.sourcePath = sourcePath_;
    result_.testIndex = 0;
    for (int i = 0; i < static_cast<int>(tests_.size()); ++i)
    {
        TestPointResult point;
        point.index = tests_[i].index; // 使用测试点的实际编号
        point.status = "NotRun"; // 初始状态为未执行
        point.executed = false;

        result_.pointResults.push_back(point); // 为每个测试点预留记录
    }
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

    // 先返回 true，等下一轮事件循环再启动编译，避免阻塞调用方
    QTimer::singleShot(0, this, &JudgeEngine::startCompile);

    return true;
}

// 启动 g++ 编译临时目录里的 main.cpp，整次判题只编译这一次。
// 步骤：确认处于编译阶段后递增 stepId_、重置结算标记；把工作目录切到临时目录，
// 将工具链 bin 目录加到 PATH 最前面（并把 TEMP/TMP 指向临时目录），最后开计时器并异步执行 g++。
void JudgeEngine::startCompile()
{
    if (stage_ != STAGE_COMPILING || workDir_ == nullptr)
        return;

    ++stepId_;
    stepSettled_ = false;

    process_->setWorkingDirectory(workDir_->path());

    // 分开收标准输出和错误输出，方便后续读取
    process_->setProcessChannelMode(QProcess::SeparateChannels);

    // 把编译器所在目录放到 PATH 开头，确保能找到配套的 cc1plus、ld 和运行时 DLL
    QString compilerPath = QString::fromUtf8(compilerPath_.c_str());

    QString compilerBin = QFileInfo(compilerPath).absolutePath();

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    env.insert("PATH", compilerBin + QDir::listSeparator() + env.value("PATH"));

    env.insert("TEMP", workDir_->path());
    env.insert("TMP", workDir_->path());

    process_->setProcessEnvironment(env);

    // 统一按 C++17 标准把 main.cpp 编译成 main.exe
    QStringList args;
    args << "-std=c++17";
    args << "main.cpp";
    args << "-o";
    args << "main.exe";

    // 给计时器绑定当前步骤号，超过 compileTimeoutMs_ 就触发编译超时
    timerStepId_ = stepId_;
    timer_->start(compileTimeoutMs_);

    process_->start(compilerPath, args);
}

// 启动学生编译出来的 main.exe，运行第 currentIndex_ 个测试点。
// 步骤：检查阶段、测试点下标和 main.exe 是否存在；清空本点专用的超时、超限、输出缓冲和耗时表；
// 配置好工作目录与工具链 PATH 后，异步启动 main.exe（真正启动后再在 onProcessStarted 里喂输入和计时）。
void JudgeEngine::startRun()
{
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

    QString exePath = workDir_->filePath("main.exe");

    if (!QFileInfo(exePath).isFile())
    {
        stepSettled_ = true;
        result_.status = "InternalError";
        result_.error = "找不到编译生成的程序";
        finishJudge();
        return;
    }

    // 重置当前测试点的运行标志和输出缓存
    timedOut_ = false;
    outputLimitExceeded_ = false;
    ioError_ = false;
    ioErrorMessage_.clear();
    actualOutput_.clear();
    runtimeError_.clear();
    runClock_.invalidate();

    // 同样把工具链 bin 目录放进 PATH 开头，保证在没装过 MinGW 的电脑上也能找到运行库 DLL
    process_->setWorkingDirectory(workDir_->path());

    QString compilerPath = QString::fromUtf8(compilerPath_.c_str());
    QString compilerBin = QFileInfo(compilerPath).absolutePath();
    QProcessEnvironment runEnv = QProcessEnvironment::systemEnvironment();
    runEnv.insert("PATH", compilerBin + QDir::listSeparator() + runEnv.value("PATH"));
    process_->setProcessEnvironment(runEnv);

    process_->setProcessChannelMode(QProcess::SeparateChannels);

    // 启动 main.exe，等它发出 started 信号后再开始计时和写入测试输入
    process_->start(exePath);
}

// 一次判题的统一收尾函数：清理资源并通知界面。
// 步骤：如果已经在空闲或清理阶段就直接跳过；先确认子进程确实退出了（如果 1 秒还退不出，
// 就记系统错误并保持引擎锁定，不发完成信号，免得和下次任务冲突）；然后停计时器、合并清理报错、
// 删除临时目录、清空测试点并切回空闲，最后发出一次 judgeFinished() 信号。
void JudgeEngine::finishJudge()
{
    if (stage_ == STAGE_IDLE || stage_ == STAGE_CLEANING)
        return;

    // 进程如果还没彻底退出，先等 1 秒；实在退不掉就保持锁定，防止文件被占用
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

    // 前面清理编译进程树出错的话，统一记成 InternalError
    if (!cleanupError_.empty())
    {
        result_.status = "InternalError";
        if (!result_.error.empty())
            result_.error += "；";
        result_.error += cleanupError_;
    }

    // 删掉临时工作目录并释放指针；就算删除失败也继续切回空闲，不卡死后面的判题
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

    result_.passedCount = passedCount_; // 将内部通过数写入返回结果
    result_.executedCount = executedCount_; // 将实际启动过的点数写入返回结果
    result_.totalCount = static_cast<int>(tests_.size()); // 保存全部测试点数量

    tests_.clear();
    stage_ = STAGE_IDLE;

    emit judgeFinished();
}

// 编译超时时清理整棵编译器进程树（g++ 编译时会拉起 cc1plus、as、ld 等子进程，直接 kill 父进程容易留残留）。
// 步骤：先趁父进程还没停拿到 PID，用 Windows 的 taskkill /PID <pid> /T /F 强杀整棵树；
// 如果 taskkill 本身失败或超过 5 秒卡住，就先杀掉 killer、记下失败原因到 cleanupError_，再兜底调用 process_->kill()。
void JudgeEngine::terminateCompileTree()
{
    // 必须在 kill 之前取 PID，进程停掉后就拿不到了
    long long pid = (long long)process_->processId();
    bool taskkillOk = false;
    string killDetail;

    if (pid > 0)
    {
        QProcess killer;
        QStringList killArgs;
        // /T 表示连子进程一起杀，/F 表示强制结束
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

    // taskkill 没成功且 g++ 还在跑，记下原因并用 kill() 兜底
    if (!taskkillOk && process_->state() != QProcess::NotRunning)
    {
        cleanupError_ = "编译超时，清理编译器进程树失败";
        if (!killDetail.empty())
            cleanupError_ += "（" + killDetail + "）";
        process_->kill();
    }
}

// 子进程（g++ 或 main.exe）退出后由 QProcess 触发；exitCode 是退出码，exitStatus 表示正常退出还是崩溃。
// 步骤：用 stepSettled_ 保证同一步骤只结算一次，停掉计时器后分两种阶段处理：
// 1) 编译阶段：收完残留输出，依次排查超时、超限、读写错、崩溃、非 0 退出（区分工具链坏了还是源码 CE）和缺 main.exe；
//    全都通过则切到运行阶段，从第 0 个测试点开始跑。
// 2) 运行阶段：算出本点耗时并累加，收完残留输出，按“超时 -> 输出超限 -> 读写错 -> 崩溃/非 0 退出 -> 输出比对”判定；
//    只要没 AC 就立刻停并收尾，AC 且还有下一点就继续跑，全过则把总状态记为 AC（testIndex 记 0）并收尾。
void JudgeEngine::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (stage_ != STAGE_COMPILING && stage_ != STAGE_RUNNING)
        return;

    // 防止同一步骤被重复结算
    if (stepSettled_)
        return;
    stepSettled_ = true;

    timer_->stop();

    // 分支一：g++ 编译结束
    if (stage_ == STAGE_COMPILING)
    {
        // 把管道里还没来得及读完的输出收干净
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
                // 非 0 退出时先排除了工具链故障，剩下的才是学生源码编译错误 CE
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
            // 编译通过，整次任务只编译这一次，切到运行阶段从第 1 个测试点（下标 0）开始
            currentIndex_ = 0;
            stage_ = STAGE_RUNNING;

            QTimer::singleShot(0, this, &JudgeEngine::startRun);

            return;
        }

        finishJudge();
        return;
    }

    // 分支二：学生程序跑完当前测试点
    if (stage_ == STAGE_RUNNING)
    {
        // 统计本测试点耗时并加到总耗时里
        long long pointElapsedMs = 0;
        if (runClock_.isValid())
        {
            pointElapsedMs = runClock_.elapsed();
        }
        totalElapsedMs_ += pointElapsedMs;

        // 收完当前测试点管道里剩余的输出
        onStdoutReady();
        onStderrReady();

        string pointActual = actualOutput_.toStdString();
        string pointExpected = tests_[currentIndex_].expectedOutput;
        string pointStderr = runtimeError_.toStdString();
        string pointStatus;

        // 1) 先看超时：因为超时后主动 kill 会产生非 0 退出码，必须排在 RE 前面判断
        if (timedOut_)
        {
            pointStatus = "TLE";
            if (!pointStderr.empty())
                pointStderr += "\n";
            pointStderr += "程序运行超时（时限 " + to_string(problem_.timeLimitMs) + " ms）";
        }
        // 2) 输出超 16 MiB 直接报 OutputLimit，不能拿被截断的输出去判 AC/WA
        else if (outputLimitExceeded_)
        {
            pointStatus = "OutputLimit";
            result_.error = "程序输出超过 16 MiB 上限";
        }
        // 3) 进程管道读写出错算系统错误，同样不能拿不完整输出去比对
        else if (ioError_)
        {
            pointStatus = "InternalError";
            result_.error = ioErrorMessage_;
        }
        // 4) 程序崩溃或返回非 0 退出码判为 RE（返回 0 但写过 cerr 不算 RE）
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
        // 5) 正常退出后，用基类指针 OutputJudge* 指向 ExactJudge 多态调用 compare() 比对输出
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

        TestPointResult& point = result_.pointResults[currentIndex_]; // 引用集合中的当前记录
        point.status = pointStatus; // 保存本点状态
        point.elapsedMs = pointElapsedMs; // 保存本点耗时
        point.runtimeError = pointStderr; // 保存运行错误说明
        point.error = result_.error; // 保存本点错误信息

        // 首错即停：当前点没过就记下该点信息并结束本次判题
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

        ++passedCount_;

        // 当前点通过，还有后续测试点就异步启动下一个
        if (currentIndex_ + 1 < (int)tests_.size())
        {
            ++currentIndex_;
            QTimer::singleShot(0, this, &JudgeEngine::startRun);
            return;
        }

        // 全部测试点都通过，总状态记 AC，testIndex 记 0，输出字段保留最后一个点
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

// 子进程启动失败或通信出错时由 QProcess 触发，error 是具体的错误码。
// 步骤：只处理尚未结算的编译或运行阶段。如果是 FailedToStart（根本没拉起来），不会再触发 finished，
// 所以直接记为 InternalError 并收尾；如果是管道读写出错，设好 ioError_ 后强杀进程，交给 onProcessFinished 统一处理；
// 其他错误（比如 Crashed）也会触发 finished，留给 onProcessFinished 判断。
void JudgeEngine::onProcessError(QProcess::ProcessError error)
{
    if (stage_ != STAGE_COMPILING && stage_ != STAGE_RUNNING)
    {
        return;
    }
    if (stepSettled_)
    {
        return;
    }

    // 进程没启动成功不会触发 finished，直接按系统错误收尾
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
        if (stage_ == STAGE_RUNNING)
        {
            TestPointResult& point = result_.pointResults[currentIndex_];
            point.status = "InternalError"; // 被测程序启动失败
            point.error = result_.error;
        }
        if (process_->state() == QProcess::NotRunning)
        {
            finishJudge();
        }

        return;
    }

    // 管道读写失败，标记后强杀，避免用残缺输出判题
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
}

// 编译或单个测试点运行超时后由 QTimer 触发。
// 步骤：先核对当前阶段、结算标记和步骤号（timerStepId_ == stepId_），拦住上一个点迟到的定时器；
// 确认进程还在跑后置 timedOut_ = true，编译超时调用 terminateCompileTree() 清进程树，
// 运行超时直接 kill() 学生程序，然后等进程触发 finished 信号再统一结算。
void JudgeEngine::onTimeout()
{
    if (stage_ != STAGE_COMPILING && stage_ != STAGE_RUNNING)
    {
        return;
    }

    // 核对步骤号，避免前一个测试点迟到的超时信号误杀正在跑的新点
    if (stepSettled_ || timerStepId_ != stepId_)
    {
        return;
    }

    if (process_->state() == QProcess::NotRunning)
        return;

    // 先标超时再杀进程，这样 onProcessFinished 能认出是超时而不是 RE
    timedOut_ = true;

    if (stage_ == STAGE_COMPILING)
    {
        terminateCompileTree();
    }
    else
    {
        process_->kill();
    }

    // 不在这里直接 finishJudge()，等进程退出触发 onProcessFinished 后统一收尾
}

// 子进程真正启动成功时由 QProcess 触发。
// 步骤：只在运行学生程序且本点未结算时处理——已执行点数加 1，启动运行耗时表和单点超时计时器，
// 再把当前测试点的标准输入写给程序并立刻关掉写端，让循环读到文件尾（EOF）的程序能正常结束。
void JudgeEngine::onProcessStarted()
{
    if (stage_ != STAGE_RUNNING || stepSettled_)
        return;

    // 确认程序真正跑起来了，才计入已执行点数并开始算运行时间和超时
    ++executedCount_;
    result_.pointResults[currentIndex_].executed = true; // 当前点成功启动

    runClock_.start();

    timerStepId_ = stepId_;
    timer_->start(problem_.timeLimitMs);

    // 写入当前测试点输入后马上关掉写通道，发 EOF 给学生程序（否则 while(cin >> x) 会一直等）
    QByteArray input = QByteArray::fromStdString(tests_[currentIndex_].input);

    process_->write(input);

    process_->closeWriteChannel();
}

// 子进程标准输出有新数据时由 QProcess 触发，转交 collectOutput(false) 读取。
void JudgeEngine::onStdoutReady()
{
    collectOutput(false);
}

// 子进程错误输出有新数据时由 QProcess 触发，转交 collectOutput(true) 读取。
void JudgeEngine::onStderrReady()
{
    collectOutput(true);
}

// 读取子进程输出并做 16 MiB 容量保护，fromStderr 为 true 时读错误输出，为 false 时读标准输出。
// 算法：先确定目标缓冲区和已用字节数（运行时 stdout 和 stderr 合起来算总限额），算出剩余额度 remaining，
// 每次最多读 remaining + 1 字节；只要读到的长度超过 remaining，就说明超限了，
// 只截留限额以内的部分，把 outputLimitExceeded_ 置为 true 并马上杀掉进程，防止死循环打印撑爆内存。
void JudgeEngine::collectOutput(bool fromStderr)
{
    if (stage_ != STAGE_COMPILING && stage_ != STAGE_RUNNING)
        return;

    if (outputLimitExceeded_)
        return;

    // 单次编译或单点运行最多收 16 MiB 输出
    const long long limit = 16 * 1024 * 1024;

    // 选中对应的读通道，并确定写入哪个缓冲区以及当前已占用的总字节数
    if (fromStderr)
    {
        process_->setReadChannel(QProcess::StandardError);
    }
    else
    {
        process_->setReadChannel(QProcess::StandardOutput);
    }

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

    // 多尝试读 1 字节：只要长度大于 remaining 就说明已经超限
    QByteArray data = process_->read(remaining + 1);

    if (data.size() > remaining)
    {
        // 只保留限额内的内容，立刻标记超限并终止进程
        target->append(data.left(remaining));

        outputLimitExceeded_ = true;

        if (process_->state() != QProcess::NotRunning)
            process_->kill();

        return;
    }

    target->append(data);
}

// 根据编译器的输出文字 compilerOutput 判断是不是工具链本身出问题，是返回 true，普通语法错返回 false。
// 算法：分两步判断——先扫描整个字符串看有没有非空白字符，如果编译器非 0 退出却什么提示都没打，
// 通常是缺运行库 DLL 或环境损坏；再拿一组常见工具链报错关键字（如找不到 cc1plus/ld、CreateProcess 失败等）
// 逐个查找，命中任一条就认定是工具链故障。
bool JudgeEngine::isToolchainFailure(const string &compilerOutput) const
{
    // 第一步：全空白输出说明编译器没机会打印语法诊断，大概率是环境或 DLL 缺失
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

    // 第二步：匹配编译器内部组件缺失、无法拉起子程序或内部崩溃的常见英文提示
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
