#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QTextEdit>
#include "problem/problem_library.h"
#include "compiler/compiler_checker.h"
#include "judge/judge_engine.h"
#include "progress/progress_store.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent*event) override; // 处理窗口关闭请求

private slots:
    void importPackage();
    void refreshProblems();
    void showSelectedProblems(int row,int column);
    void checkCompiler();
    void onCompilerCheckFinished();
    void chooseSource();// 选择本地 cpp 源码
    void startJudge();// 发起判题任务
    void onJudgeFinished(); // 接收判题完成信号

private:
    Ui::MainWindow *ui;
    ProblemLibrary* library_;
    CompilerChecker*checker_;

    QTextEdit* statementText_;
    QPushButton* checkCompilerButton_;

    JudgeEngine* judgeEngine_;// 保存 B 的判题引擎对象
    Problem currentProblem_;// 保存当前选中的题目
    QString sourcePath_; // 保存当前选择的源码路径
    bool hasSelectedProblem_; // 表示当前是否已经选择题目
    ProgressStore* progressStore_;//保存进度存储对象
    Progress currentProgress_; //保存当前题目的进度
    QString userId_;//保存当前练习用户ID
};

#endif // MAINWINDOW_H
