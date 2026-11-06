#ifndef JUDGE_ENGINE_H
#define JUDGE_ENGINE_H

#include <QByteArray>
#include <QObject>
#include <QProcess>

#include <memory>
#include <string>
#include <vector>
#include<QElapsedTimer>

#include "common/types.h"

class QTimer;
class QTemporaryDir;

class JudgeEngine : public QObject
{
    Q_OBJECT

  public:
    JudgeEngine(std::string compilerPath, QObject *parent = nullptr);

    ~JudgeEngine() override;

    bool startJudge(Problem problem, std::string sourcePath, std::vector<TestCase> tests, std::string &error);

    JudgeResult getResult();
    bool isBusy() const;

  signals:
    void judgeFinished();

  private:
    enum class Stage
    {
        Idle,
        Compiling,
        Running,
        Cleaning
    };

    Stage stage_ = Stage::Idle;

    std::string compilerPath_;

    QProcess *process_ = nullptr;
    QTimer *timer_ = nullptr;

    std::unique_ptr<QTemporaryDir> workDir_;

    Problem problem_;
    TestCase test_;
    std::string sourcePath_;

    JudgeResult result_;
    bool timedOut_ = false;

    QByteArray compilerOutput_;
    QByteArray actualOutput_;
    QByteArray runtimeError_;

    void startCompile();
    void startRun();
    void finishJudge();

    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onTimeout();
    void onProcessStarted();

    QElapsedTimer runClock_;
    bool outputLimitExceeded_ = false;

    void onStdoutReady();
    void onStderrReady();
    void collectOutput(bool fromStderr);
};

#endif