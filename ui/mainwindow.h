#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QTextEdit>
#include "problem/problem_library.h"
#include "compiler/compiler_checker.h"
#include "judge/judge_engine.h"
#include "progress/progress_store.h"
#include "account/account_manager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    bool isStartupReady() const; // 返回主窗口是否完成启动

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
    void clearSource();//清除当前题目的源码绑定
    void logoutAccount();// 处理退出账号按钮
    void openFilterDialog(); // 打开筛选对话框
    void removeSelectedPackage(); // 移除当前选中的题包

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
    Progress judgingProgress_; // 发起任务时锁定的进度副本。
    Progress currentProgress_; //保存当前题目的进度
    QString userId_;//保存当前练习用户ID
    AccountManager* accountManager_; // 保存账号管理对象
    QString currentUserId_; // 保存当前登录用户名
    bool showLoginDialog(); // 显示登录窗口并完成账号登录
    bool startupReady_; // 保存启动流程是否成功
    QString titleFilter_;// 保存标题筛选条件
    QString tagFilter_;// 保存标签筛选条件
    QString statusFilter_;// 保存状态筛选条件
    QString packageFilter_;// 保存题包筛选条件
    QString selectedPackageId_; // 保存当前选中的题包编号
};

#endif // MAINWINDOW_H
