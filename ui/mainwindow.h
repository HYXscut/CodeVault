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

/*
 类说明：主窗口类，作为 CodeVault 系统的核心界面与业务中枢，整合账号会话、题库浏览与筛选、共享题包管理、本地编译器环境检查、题目判题评测调度以及刷题进度持久化存储等功能。
 继承与派生：公有继承自 QMainWindow，具备标准主窗口的菜单、工具栏、中心部件及事件处理能力。
 属性说明：
    ui: Qt Designer 生成的主界面控件管理对象指针（Ui::MainWindow*）
    library_: 题库管理对象指针（ProblemLibrary*）
    checker_: 编译器环境检查对象指针（CompilerChecker*）
    statementText_: 题面 Markdown 渲染展示控件指针（QTextEdit*）
    checkCompilerButton_: 工具栏编译器状态检查按钮指针（QPushButton*）
    judgeEngine_: 判题调度引擎对象指针（JudgeEngine*）
    currentProblem_: 当前在题目列表中选中的题目数据对象（Problem）
    sourcePath_: 当前题目绑定的本地 C++ 源码文件路径（QString）
    hasSelectedProblem_: 标记当前是否有题目被选中（bool）
    progressStore_: 练习进度持久化存储管理对象指针（ProgressStore*）
    judgingProgress_: 发起判题评测任务时锁定的做题进度快照副本（Progress）
    currentProgress_: 当前选中题目的最新有效做题进度（Progress）
    userId_: 当前登录用户的内部稳定身份 ID，用于进度归属与存储（QString）
    accountManager_: 账号认证与会话管理对象指针（AccountManager*）
    currentUserId_: 当前登录用户的显示用户名（QString）
    startupReady_: 标记系统启动初始化与登录流程是否已就绪（bool）
    titleFilter_: 标题搜索关键字筛选条件（QString）
    tagFilter_: 标签类型筛选条件（QString）
    statusFilter_: 判题状态筛选条件（QString）
    packageFilter_: 所属题包筛选条件（QString）
    selectedPackageId_: 当前选中的题包编号（QString）
 方法说明：
    MainWindow(): 构造函数，完成各子系统初始化、界面构建与会话登录
    ~MainWindow(): 析构函数，释放动态分配的各子模块资源
    isStartupReady(): 查询主窗口启动与会话登录是否成功就绪
    closeEvent(): 拦截窗口关闭事件，在评测进行中安全阻断并提示用户
    importPackage(): 导入新题包或恢复已移除题包的槽函数
    refreshProblems(): 根据当前筛选条件刷新并展示题目列表的槽函数
    showSelectedProblems(): 响应题目列表行选中切换，展示题面与进度的槽函数
    checkCompiler(): 触发异步编译器环境与工具链检查的槽函数
    onCompilerCheckFinished(): 响应编译器检查完成信号并更新状态展示的槽函数
    chooseSource(): 打开文件对话框选择本地 C++ 源码文件的槽函数
    startJudge(): 提交并启动题目判题评测任务的槽函数
    onJudgeFinished(): 响应评测结束信号，更新逐点结果并原子保存进度的槽函数
    clearSource(): 清除当前题目的本地源码绑定的槽函数
    logoutAccount(): 处理退出登录或切换账号请求的槽函数
    openFilterDialog(): 打开题目多条件组合筛选对话框的槽函数
    removeSelectedPackage(): 移除（隐藏）当前选中题包的槽函数
    showLoginDialog(): 私有辅助函数，弹出登录对话框并完成账号身份校验
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    // 创建主窗口对象
    // 参数说明：parent - 父窗口指针，默认为 nullptr
    explicit MainWindow(QWidget *parent = nullptr);

    // 释放界面资源并析构对象
    ~MainWindow() override;

    // 查询启动就绪状态
    // 返回值：启动且登录完成返回 true，否则返回 false
    bool isStartupReady() const; // 返回主窗口是否完成启动

protected:
    // 窗口关闭事件处理
    // 参数说明：event - 包含关闭意图的事件对象
    void closeEvent(QCloseEvent*event) override; // 处理窗口关闭请求

private slots:
    // 导入外部题包
    // 功能说明：响应导入题包按钮点击，选择文件夹导入题库
    void importPackage();

    // 刷新题目列表
    // 功能说明：根据当前筛选条件刷新并重新填充题目表格
    void refreshProblems();

    // 展示选中题目详情
    // 参数说明：row - 选中的表格行；column - 选中的表格列
    void showSelectedProblems(int row,int column);

    // 检查编译器环境
    // 功能说明：触发异步编译器可用性与版本检查
    void checkCompiler();

    // 编译器检查完成槽函数
    // 功能说明：接收编译器检查完成信号并更新界面状态
    void onCompilerCheckFinished();

    // 选择本地源码文件
    // 功能说明：打开文件对话框选择 C++ 源码并关联到当前题目
    void chooseSource();// 选择本地 cpp 源码

    // 发起题目判题
    // 功能说明：准备测试点数据并启动评测引擎
    void startJudge();// 发起判题任务

    // 接收判题完成结果
    // 功能说明：展示评测数据、逐点信息并保存练习进度
    void onJudgeFinished(); // 接收判题完成信号

    // 清除源码绑定
    // 功能说明：清空当前题目的本地源码关联并同步保存进度
    void clearSource();//清除当前题目的源码绑定

    // 退出当前账号
    // 功能说明：注销会话、清空界面状态并重新打开登录窗口
    void logoutAccount();// 处理退出账号按钮

    // 打开筛选对话框
    // 功能说明：弹出多条件组合筛选窗口
    void openFilterDialog(); // 打开筛选对话框

    // 移除选中的题包
    // 功能说明：确认后从共享题库中隐藏当前选中的题包
    void removeSelectedPackage(); // 移除当前选中的题包

private:
    // Qt Designer 生成的主界面控件指针
    Ui::MainWindow *ui;
    // 题库管理对象指针
    ProblemLibrary* library_;
    // 编译器检查对象指针
    CompilerChecker*checker_;

    // 题面文本显示控件指针
    QTextEdit* statementText_;
    // 编译器检查按钮指针
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
    // 显示登录窗口
    // 返回值：登录成功返回 true，取消或失败返回 false
    bool showLoginDialog(); // 显示登录窗口并完成账号登录
    bool startupReady_; // 保存启动流程是否成功
    QString titleFilter_;// 保存标题筛选条件
    QString tagFilter_;// 保存标签筛选条件
    QString statusFilter_;// 保存状态筛选条件
    QString packageFilter_;// 保存题包筛选条件
    QString selectedPackageId_; // 保存当前选中的题包编号
};

#endif // MAINWINDOW_H
