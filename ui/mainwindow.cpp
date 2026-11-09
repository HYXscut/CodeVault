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


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow),
    library_(nullptr),
    checker_(nullptr),
    statementText_(nullptr),
    checkCompilerButton_(nullptr)
{
    ui->setupUi(this);
    ui->statusLabel->setText(QStringLiteral("Qt 界面工程已加载，等待连接业务功能"));

    QString appDir=QCoreApplication::applicationDirPath();//获得所在目录的绝对路径

    QString packagesDir=QDir(appDir).filePath("data/packages");//把程序所在目录 appDir 和相对子路径 "data/packages" 拼接成一个完整的目录路径，存到 packagesDir 中

    QString compilerPath=QDir(appDir).filePath("toolchain/bin/g++.exe");

    library_=new ProblemLibrary(packagesDir.toStdString());

    checker_=new CompilerChecker(compilerPath.toStdString(),this);

    ui->statusLabel->setText(QStringLiteral("程序初始化完成"));


    connect(ui->importButton,&QPushButton::clicked,this,&MainWindow::refreshProblems);

    connect(ui->refreshButton,&QPushButton::clicked,this,&MainWindow::refreshProblems);

    connect(ui->problemTable,&QTableWidget::cellClicked,this,&MainWindow::showSelectedProblems);

    connect(ui->checkCompilerButton,&QPushButton::clicked,this,&MainWindow::checkCompiler);

    connect(checker_,&CompilerChecker::checkFinished,this,&MainWindow::onCompilerCheckFinished);

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

    //在题面区域显示题面
    ui->statementText->setPlainText(QString::fromUtf8(problem.statement.c_str()));
    //更新状态栏
    ui->statusLabel->setText(QStringLiteral("当前题目：%1").arg(QString::fromUtf8(problem.title.c_str())));
}
