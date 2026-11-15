#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QTableWidgetItem> // 使用表格单元格对象
#include <QFileDialog>//提供选择文件夹的对话框
#include <QFileInfo> // 检查文件信息
#include <QCloseEvent> //提供关闭事件的定义


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow),
    library_(nullptr),
    checker_(nullptr),
    statementText_(nullptr),
    checkCompilerButton_(nullptr),
    judgeEngine_(nullptr),
    hasSelectedProblem_(false)
{
    ui->setupUi(this);
    ui->checkCompilerButton->setFixedSize(140, 30);
    ui->sourcePathEdit->setReadOnly(true);
    ui->actualOutputText->setReadOnly(true);
    ui->expectedOutputText->setReadOnly(true);
    ui->errorOutputText->setReadOnly(true);
    ui->statusLabel->setText(QStringLiteral("Qt 界面工程已加载，等待连接业务功能"));

    QString appDir=QCoreApplication::applicationDirPath();//获得所在目录的绝对路径

    QString packagesDir=QDir(appDir).filePath("data/packages");//把程序所在目录 appDir 和相对子路径 "data/packages" 拼接成一个完整的目录路径，存到 packagesDir 中

    QString compilerPath=QDir(appDir).filePath("toolchain/bin/g++.exe");

    library_=new ProblemLibrary(packagesDir.toStdString());

    checker_=new CompilerChecker(compilerPath.toStdString(),this);

    judgeEngine_=new JudgeEngine(compilerPath.toStdString(),this);

    ui->statusLabel->setText(QStringLiteral("程序初始化完成"));


    connect(ui->importButton,&QPushButton::clicked,this,&MainWindow::refreshProblems);

    connect(ui->refreshButton,&QPushButton::clicked,this,&MainWindow::refreshProblems);

    connect(ui->problemTable,&QTableWidget::cellClicked,this,&MainWindow::showSelectedProblems);

    connect(ui->checkCompilerButton,&QPushButton::clicked,this,&MainWindow::checkCompiler);

    connect(checker_,&CompilerChecker::checkFinished,this,&MainWindow::onCompilerCheckFinished);

    connect(ui->chooseSourceButton,&QPushButton::clicked,this,&MainWindow::chooseSource);

    connect(ui->judgeButton,&QPushButton::clicked,this,&MainWindow::startJudge);

    connect(judgeEngine_,&JudgeEngine::judgeFinished,this,&MainWindow::onJudgeFinished);

    refreshProblems();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::refreshProblems()//函数作用：刷新题目列表
{
    std::string error;//保存题库加载失败时的错误信息

    if(!library_->load(error))//尝试加载已保存的题目
    {
        ui->problemTable->setRowCount(0);//加载失败时清空题目表格

        ui->statusLabel->setText(//设置状态栏中的错误文字
            QString::fromUtf8(error.c_str()) //将UTF8转换为QString
            );

        return;
    }

    const std::vector<Problem>problems=library_->getProblems();//定义题目列表并获取所有题目

    ui->problemTable->setRowCount(static_cast<int>(problems.size()));//通过将题目数量转换为int来设置表格的行数

    for (int row = 0; row < static_cast<int>(problems.size()); ++row) {
        const Problem& problem = // 取得当前题目的引用
            problems[static_cast<std::size_t>(row)]; // 根据行号访问题目

        auto* packageItem = // 创建题包编号单元格
            new QTableWidgetItem(
                QString::fromUtf8(
                    problem.packageId.c_str())); // 写入题包编号

        auto* problemItem = // 创建题目编号单元格
            new QTableWidgetItem(
                QString::fromUtf8(
                    problem.problemId.c_str())); // 写入题目编号

        auto* titleItem = // 创建题目标题单元格
            new QTableWidgetItem(
                QString::fromUtf8(
                    problem.title.c_str())); // 写入题目标题

        auto* statusItem = // 创建题目状态单元格
            new QTableWidgetItem(
                QStringLiteral("未开始")); // 设置默认状态为未开始

        packageItem->setData( // 给题包单元格保存额外数据
            Qt::UserRole, // 保存题包编号使用的自定义数据角色
            QString::fromUtf8( // 将编号转换为 QString
                problem.packageId.c_str())); // 保存 packageId

        packageItem->setData( // 再保存题目编号
            Qt::UserRole + 1, // 使用另一个自定义数据角色
            QString::fromUtf8( // 将编号转换为 QString
                problem.problemId.c_str())); // 保存 problemId

        ui->problemTable->setItem( // 将题包编号放入表格
            row, // 当前行
            0, // 第 0 列
            packageItem); // 放入题包编号单元格

        ui->problemTable->setItem( // 将题目编号放入表格
            row,
            1,
            problemItem); // 放入题目编号单元格

        ui->problemTable->setItem( // 将题目标题放入表格
            row,
            2,
            titleItem); // 放入标题单元格

        ui->problemTable->setItem( // 将题目状态放入表格
            row,
            3,
            statusItem); // 放入状态单元格
    }
    ui->statusLabel->setText( // 更新状态栏
        QStringLiteral("已加载 %1 道题目") // 设置提示文字格式
            .arg(problems.size())); // 将题目数量填入提示文字
}

void MainWindow::importPackage()//函数作用：导入题包
{
    const QString folder=QFileDialog::getExistingDirectory(this,QStringLiteral("选择题包目录"));
    //保存用户选择的题包目录并打开文件夹选择对话框

    if(folder.isEmpty())
    {
        return;
    }

    std::string error;

    const bool success=library_->importPackage(
        folder.toStdString(),error
        );//将错误信息写入error

    if(!success)
    {
        //导入失败显示错误
        ui->statusLabel->setText(QString::fromUtf8(error.c_str()));
        return;
    }

    refreshProblems();

    ui->statusLabel->setText(QStringLiteral("题包导入成功"));
}

void MainWindow::showSelectedProblems(int row,int column)//函数作用：点击题目后处理
{
    Q_UNUSED(column);

    QTableWidgetItem* item=ui->problemTable->item(row,0);

    if(item==nullptr)//判断单元格是否为空
    {
        return;
    }

    //保存题包编号
    const std::string packageId=item->data(Qt::UserRole).toString().toStdString();
    //保存题目编号
    const std::string problemId=item->data(Qt::UserRole+1).toString().toStdString();

    Problem problem;

    std::string error;

    const bool success=library_->findProblem(packageId,problemId,problem,error);

    if(!success)
    {
        //输出错误信息
        ui->statementText->setPlainText(error.c_str());

        return;
    }

    currentProblem_=problem;
    hasSelectedProblem_=true;
    ui->judgeButton->setEnabled(!sourcePath_.isEmpty());

    //在题面区域显示题面
    ui->statementText->setPlainText(QString::fromUtf8(problem.statement.c_str()));
    //更新状态栏
    ui->statusLabel->setText(QStringLiteral("当前题目：%1").arg(QString::fromUtf8(problem.title.c_str())));
}

void MainWindow::checkCompiler()
{
    if(!checker_->checkCompiler()) // 调用 B 的接口，判断请求是否被接受
    {
        ui->statusLabel->setText(QStringLiteral("已有检查正在进行"));
        return;
    }
    ui->checkCompilerButton->setEnabled(false);
    ui->statusLabel->setText(QStringLiteral("正在检查编译器…"));
}

void MainWindow::onCompilerCheckFinished()
{
    const CompilerInfo info=checker_->getCompilerInfo();//读取本次检查结果

    if(info.found)
    {
        //成功时显示版本
        ui->statusLabel->setText(QString::fromUtf8(info.version.c_str()));
    }
    else
    {
        //失败时显示错误
        ui->statusLabel->setText(QString::fromUtf8(info.error.c_str()));
    }

    ui->checkCompilerButton->setEnabled(true);//检查结束后恢复按钮
}

void MainWindow::chooseSource()
{
    const QString filePath=QFileDialog::getOpenFileName(this,QStringLiteral("选择C++源码"),QString(),QStringLiteral("C++源码(*.cpp)"));// 保存用户选择的文件路径
    if(filePath.isEmpty())// 判断用户是否取消选择
    {
        return;
    }
    const QFileInfo fileInfo(filePath);

    // 判断选择的路径是否为普通文件
    if(!fileInfo.isFile())
    {
        ui->statusLabel->setText(QStringLiteral("选择的路径不是文件"));
        return;
    }

    // 获取文件扩展名并进行比较
    if(fileInfo.suffix().compare(QStringLiteral("cpp"),Qt::CaseInsensitive)!=0)
    {
        ui->statusLabel->setText(QStringLiteral("请选择.cpp源码文件"));
        return;
    }

    sourcePath_=filePath;// 保存源码路径到窗口成员变量

    ui->sourcePathEdit->setText(sourcePath_);

    ui->statusLabel->setText(QStringLiteral("源码选择成功"));

    ui->judgeButton->setEnabled(hasSelectedProblem_);
}

void MainWindow::startJudge()
{
    if(!hasSelectedProblem_)//检查是否已经选择题目
    {
        ui->statusLabel->setText(QStringLiteral("请先选择题目"));

        return;
    }

    if(sourcePath_.isEmpty())//检查是否已经选择源码
    {
        ui->statusLabel->setText(QStringLiteral("请选择cpp源码"));

        return;
    }

    TestCase testCase;
    std::string error;

    //保存测试点读取结果
    const bool testLoaded=library_->getTestCase(currentProblem_.packageId,currentProblem_.problemId,1,testCase,error);

    if(!testLoaded) //判断测试点是否读取失败
    {
        ui->statusLabel->setText(QString::fromUtf8(error.c_str()));
        return;
    }

    std::vector<TestCase> tests;//创建测试点列表

    tests.push_back(testCase);//将第一个测试点放入列表

    const bool accepted=judgeEngine_->startJudge(currentProblem_,sourcePath_.toStdString(),tests,error);

    if(!accepted) //判断判题请求是否被拒绝
    {
        ui->statusLabel->setText(QString::fromUtf8(error.c_str()));

        return;
    }

    //禁止判题期间重复判题、重新选择源码、导入题包、刷新题库、检查编译器
    ui->judgeButton->setEnabled(false);
    ui->chooseSourceButton->setEnabled(false);
    ui->importButton->setEnabled(false);
    ui->refreshButton->setEnabled(false);
    ui->checkCompilerButton->setEnabled(false);
    ui->problemTable->setEnabled(false);
}

void MainWindow::onJudgeFinished()
{
    const JudgeResult result=judgeEngine_->getResult();

    ui->resultLabel->setText(QString::fromUtf8(result.status.c_str()));

    ui->actualOutputText->setPlainText(QString::fromUtf8(result.actualOutput.c_str()));

    ui->expectedOutputText->setPlainText(QString::fromUtf8(result.expectedOutput.c_str()));

    QString errorText=QString::fromUtf8(result.compilerOutput.c_str());

    if(errorText.isEmpty())
    {
        errorText=QString::fromUtf8(result.error.c_str());
    }

    ui->errorOutputText->setPlainText(errorText);
    ui->statusLabel->setText( // 更新状态栏
        QStringLiteral("判题完成，耗时 %1 ms") // 设置耗时提示格式
            .arg(result.elapsedMs)); // 填入实际耗时
    // 恢复判题、源码选择、导入、刷新、编译器检查、切换按钮
    ui->judgeButton->setEnabled(true);
    ui->chooseSourceButton->setEnabled(true);
    ui->importButton->setEnabled(true);
    ui->refreshButton->setEnabled(true);
    ui->checkCompilerButton->setEnabled(true);
    ui->problemTable->setEnabled(true);

}

void MainWindow::closeEvent(QCloseEvent*event)
{
    if(judgeEngine_->isBusy())
    {
        //提示用户等待
        ui->statusLabel->setText(QStringLiteral("判题进行中，请等待结束后再关闭"));

        event->ignore();//拒绝本次关闭请求
        return;
    }
    QMainWindow::closeEvent(event);
}








