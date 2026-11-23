#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "logindialog.h"
#include "filterdialog.h"
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
#include <QCloseEvent> // 提供关闭事件的定义
#include <QDateTime> //获取当前时间
#include <QFile> // 读写隐藏题包记录
#include <QTextStream> // 按行读写文本
#include <QAbstractItemView>//使用表格选择和编辑模式
#include <QHeaderView>//使用表格表头

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow),
    library_(nullptr),
    checker_(nullptr),
    statementText_(nullptr),
    checkCompilerButton_(nullptr),
    judgeEngine_(nullptr),
    hasSelectedProblem_(false),
    progressStore_(nullptr),
    userId_(QStringLiteral("practice")),
    accountManager_(nullptr),
    currentUserId_(),
    startupReady_(false),
    titleFilter_(),
    tagFilter_(QStringLiteral("全部标签")),
    statusFilter_(QStringLiteral("全部状态")),
    packageFilter_(QStringLiteral("全部题包"))
{
    ui->setupUi(this);

        ui->problemTable->verticalHeader()->hide(); // 隐藏题目列表左侧的行号表头
    ui->pointResultTable->verticalHeader()->hide(); // 隐藏逐点结果表左侧行号和左上角黑块
    ui->pointResultTable->setColumnCount(4);//设置测试点结果表的列数
    ui->pointResultTable->setHorizontalHeaderLabels({
        QStringLiteral("测试点"),
        QStringLiteral("状态"),
        QStringLiteral("耗时"),
        QStringLiteral("说明")
    });//设置表头文字
    ui->pointResultTable->setRowCount(0);//程序启动时暂时没有测试点结果
    ui->pointResultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);//设置表格编辑方式
    ui->pointResultTable->setSelectionBehavior(QAbstractItemView::SelectRows);//点击时选择整行
    ui->pointResultTable->setSelectionMode(QAbstractItemView::SingleSelection);//一次只能选择一行
    ui->pointResultTable->horizontalHeader()->setStretchLastSection(true);//让说明列自动填充剩余宽度
    ui->countLabel->setText(QStringLiteral("通过0，已执行0，总点数0"));//显示初始计数

    ui->checkCompilerButton->setFixedSize(140, 30);
    ui->sourcePathEdit->setReadOnly(true);
    ui->actualOutputText->setReadOnly(true);
    ui->expectedOutputText->setReadOnly(true);
    ui->errorOutputText->setReadOnly(true);
    ui->statusLabel->setText(QStringLiteral("Qt 界面工程已加载，等待连接业务功能"));

    QString appDir=QCoreApplication::applicationDirPath();//获得所在目录的绝对路径

    QString packagesDir=QDir(appDir).filePath("data/packages");//把程序所在目录 appDir 和相对子路径 "data/packages" 拼接成一个完整的目录路径，存到 packagesDir 中

    QString compilerPath=QDir(appDir).filePath("toolchain/bin/g++.exe");

    const QString progressPath=QDir(appDir).filePath("data/progress.txt"); // 进度文件路径

    const QString accountPath = QDir(appDir).filePath( "data/accounts.txt"); // 保存账号文件路径

    removedPackagesPath_ = QDir(appDir).filePath("data/removed_packages.txt"); // 保存隐藏题包记录路径

    loadRemovedPackages(removedPackagesPath_); // 程序启动时恢复隐藏题包

    library_=new ProblemLibrary(packagesDir.toStdString());

    checker_=new CompilerChecker(compilerPath.toStdString(),this);

    judgeEngine_=new JudgeEngine(compilerPath.toStdString(),this);

    progressStore_=new ProgressStore(progressPath.toStdString());

    accountManager_ = new AccountManager( accountPath.toStdString());

    // 保存账号文件加载错误
    std::string accountError;
    // 加载账号文件
    if (!accountManager_->load(accountError))
    {
        // 显示账号文件错误
        QMessageBox::critical(
            this,
            QStringLiteral("账号文件错误"),
            QString::fromUtf8(accountError.c_str()));
        // 停止窗口初始化
        close();

        return;
    }

    // 尝试完成登录
    if (!showLoginDialog())
    {
        // 登录取消或失败时关闭主窗口
        close();
        return;
    }

    // 保存进度文件加载错误
    std::string progressError;

    const bool progressLoaded=progressStore_->load(progressError); // 加载进度文件

    // 判断进度文件是否加载失败
    if (!progressLoaded)
    {
        QMessageBox::critical(
            this,
            QStringLiteral("进度文件加载失败"),
            QString::fromUtf8(progressError.c_str()) +
                QStringLiteral("\n原文件未修改。请检查文件后重新启动。"));

        return; // 初始化未完成，不进入主界面
    }

    if (progressLoaded) // 只有进度文件加载成功时显示正常初始化提示
    {
        ui->statusLabel->setText(QStringLiteral("程序初始化完成")); // 显示初始化完成
    }

    connect(ui->importButton,&QPushButton::clicked,this,&MainWindow::importPackage);

    connect(ui->refreshButton,&QPushButton::clicked,this,&MainWindow::refreshProblems);

    connect(ui->problemTable,&QTableWidget::cellClicked,this,&MainWindow::showSelectedProblems);

    connect(ui->checkCompilerButton,&QPushButton::clicked,this,&MainWindow::checkCompiler);

    connect(checker_,&CompilerChecker::checkFinished,this,&MainWindow::onCompilerCheckFinished);

    connect(ui->chooseSourceButton,&QPushButton::clicked,this,&MainWindow::chooseSource);

    connect(ui->judgeButton,&QPushButton::clicked,this,&MainWindow::startJudge);

    connect(judgeEngine_,&JudgeEngine::judgeFinished,this,&MainWindow::onJudgeFinished);

    connect(ui->clearSourceButton,&QPushButton::clicked,this,&MainWindow::clearSource);

    connect(ui->logoutButton,&QPushButton::clicked,this,&MainWindow::logoutAccount);

    connect(ui->filterButton,&QPushButton::clicked,this,&MainWindow::openFilterDialog);

    // 连接移除题包按钮
    connect(ui->removePackageButton,&QPushButton::clicked,this,&MainWindow::removeSelectedPackage);

    refreshProblems();

    startupReady_ = true; // 登录成功并完成主窗口初始化
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::isStartupReady() const
{
    return startupReady_; // 返回启动状态
}

bool MainWindow::loadRemovedPackages(const QString& filePath)
{
    // 打开隐藏题包记录文件
    QFile file(filePath);

    // 文件不存在时视为空记录
    if (!file.exists())
    {
        return true;
    }

    // 尝试以文本方式打开文件
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return false;
    }

    // 逐行读取隐藏题包编号
    QTextStream input(&file);

    while (!input.atEnd())
    {
        const QString packageId = input.readLine().trimmed();

        // 空行不加入隐藏集合
        if (!packageId.isEmpty())
        {
            removedPackageIds_.insert(packageId);
        }
    }

    return true;
}

bool MainWindow::saveRemovedPackages(const QString& filePath) const
{
    // 创建保存目录
    QFileInfo fileInfo(filePath);
    QDir().mkpath(fileInfo.absolutePath());

    // 以覆盖方式打开隐藏题包记录文件
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return false;
    }

    // 逐行写入隐藏题包编号
    QTextStream output(&file);

    for (const QString& packageId : removedPackageIds_)
    {
        output << packageId << '\n';
    }

    return true;
}

QString MainWindow::packageIdFromFolder(const QString& folder) const
{
    // 拼接题包元数据文件路径
    QFile file(QDir(folder).filePath("package.txt"));

    // 打开题包元数据文件
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return QString();
    }

    // 跳过格式标识行
    QTextStream input(&file);
    input.readLine();

    // 读取题包编号行
    return input.readLine().trimmed();
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

    const std::vector<Problem> problems = library_->getProblems(); // 获取全部题目

    ui->problemTable->setRowCount(0); // 清空旧的显示结果

    int displayRow = 0; // 保存当前显示行号

    for (const Problem& problem : problems) // 遍历全部题目
    {
        // 判断题包是否已隐藏
        if (removedPackageIds_.contains(QString::fromUtf8(problem.packageId.c_str())))
        {
            // 隐藏已移除题包中的题目
            continue;
        }

        if (!matchesFilters(problem)) // 判断当前题目是否符合筛选条件
        {
            continue; // 不符合条件时跳过当前题目
        }

        ui->problemTable->insertRow(displayRow); // 为匹配题目增加一行

        auto* packageItem = new QTableWidgetItem( // 创建题包编号单元格
            QString::fromUtf8(problem.packageId.c_str())); // 写入题包编号

        auto* problemItem = new QTableWidgetItem( // 创建题目编号单元格
            QString::fromUtf8(problem.problemId.c_str())); // 写入题目编号

        auto* titleItem = new QTableWidgetItem( // 创建题目标题单元格
            QString::fromUtf8(problem.title.c_str())); // 写入题目标题

        Progress problemProgress; // 保存当前题目的进度

        std::string progressError; // 保存读取进度时的错误

        const bool progressLoaded = // 保存进度读取结果
            progressStore_->getProgress( // 查询当前题目的进度
                userId_.toStdString(), // 传入当前用户
                problem.packageId, // 传入题包编号
                problem.problemId, // 传入题目编号
                problemProgress, // 接收进度对象
                progressError); // 接收错误信息

        QString progressText; // 保存界面显示的状态文字

        if (!progressLoaded) // 判断进度读取是否失败
        {
            progressText = QStringLiteral("进度读取失败"); // 显示读取失败
        }
        else if (problemProgress.status == 2) // 判断是否本地通过
        {
            progressText = QStringLiteral("本地通过"); // 显示本地通过
        }
        else if (problemProgress.status == 1) // 判断是否练习中
        {
            progressText = QStringLiteral("练习中"); // 显示练习中
        }
        else // 处理未开始状态
        {
            progressText = QStringLiteral("未开始"); // 显示未开始
        }

        auto* statusItem = new QTableWidgetItem( // 创建状态单元格
            progressText); // 写入状态文字

        packageItem->setData( // 保存题包编号
            Qt::UserRole, // 使用自定义数据角色
            QString::fromUtf8(problem.packageId.c_str())); // 写入题包编号

        packageItem->setData( // 保存题目编号
            Qt::UserRole + 1, // 使用第二个自定义数据角色
            QString::fromUtf8(problem.problemId.c_str())); // 写入题目编号

        ui->problemTable->setItem( // 设置题包编号
            displayRow, // 使用当前显示行
            0, // 写入第 0 列
            packageItem); // 写入单元格

        ui->problemTable->setItem( // 设置题目编号
            displayRow, // 使用当前显示行
            1, // 写入第 1 列
            problemItem); // 写入单元格

        ui->problemTable->setItem( // 设置题目标题
            displayRow, // 使用当前显示行
            2, // 写入第 2 列
            titleItem); // 写入单元格

        ui->problemTable->setItem( // 设置题目状态
            displayRow, // 使用当前显示行
            3, // 写入第 3 列
            statusItem); // 写入单元格

        ++displayRow; // 准备下一条显示行
    }

    ui->statusLabel->setText( // 更新题目数量提示
        QStringLiteral("显示 %1 道题目").arg(displayRow)); // 显示筛选后的数量
}

void MainWindow::importPackage()//函数作用：导入题包
{
    const QString folder=QFileDialog::getExistingDirectory(this,QStringLiteral("选择题包目录"));
    //保存用户选择的题包目录并打开文件夹选择对话框

    if(folder.isEmpty())
    {
        return;
    }

    // 读取外部题包编号，用于恢复被隐藏的题包
    const QString packageId = packageIdFromFolder(folder);

    // 相同题包已经保存在本地时，重新导入表示恢复显示
    if (!packageId.isEmpty() && removedPackageIds_.contains(packageId))
    {
        removedPackageIds_.remove(packageId);

        if (!saveRemovedPackages(removedPackagesPath_))
        {
            ui->statusLabel->setText(QStringLiteral("无法保存题包显示状态"));
            return;
        }

        refreshProblems();
        ui->statusLabel->setText(QStringLiteral("题包已恢复显示"));
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

    // 保存当前题包编号
    selectedPackageId_=QString::fromUtf8(currentProblem_.packageId.c_str());

    // 选中题目后允许移除题包
    ui->removePackageButton->setEnabled(true);

    std::string progressError; //保存当前题目进度读取错误

    const bool progressLoaded=progressStore_->getProgress(userId_.toStdString(),currentProblem_.packageId,currentProblem_.problemId,currentProgress_,progressError); //保存当前题目进度读取错误

    //判断进度读取是否失败
    if(!progressLoaded)
    {

        ui->statusLabel->setText(QString::fromUtf8(progressError.c_str()));
        ui->judgeButton->setEnabled(false);
        ui->removePackageButton->setEnabled(false);

        return;
    }

    sourcePath_=QString::fromUtf8(currentProgress_.sourcePath.c_str());//恢复源码路径

    ui->sourcePathEdit->setText(sourcePath_);//更新路径输入框
    ui->resultLabel->setText(QStringLiteral("等待判题")); // 切换题目时重置提示
    if(sourcePath_.isEmpty())//判断是否没有绑定源码
    {
        ui->statusLabel->setText(QStringLiteral("当前题目尚未绑定源码"));
    }

    else if(!QFileInfo(sourcePath_).isFile())
    {
        ui->statusLabel->setText(
            QStringLiteral("源码路径已失效，请重新选择")); // 保留详细提示

        ui->resultLabel->setText(
            QStringLiteral("源码路径失效")); // 在判题按钮旁显示
    }

    else//判断源码路径依然有效
    {
        ui->statusLabel->setText(QString::fromUtf8(currentProgress_.lastSummary.c_str()));
    }

        ui->judgeButton->setEnabled(
        !sourcePath_.isEmpty() && QFileInfo(sourcePath_).isFile()); // 路径非空且指向普通文件才允许判题

    //在题面区域显示题面
    ui->statementText->setPlainText(QString::fromUtf8(problem.statement.c_str()));
    
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

    Progress updatedProgress=currentProgress_; //复制当前进度作为候选数据

    updatedProgress.userId=userId_.toStdString();// 保存当前用户ID

    updatedProgress.packageId=currentProblem_.packageId;//保存当前题目的题包编号

    updatedProgress.problemId=currentProblem_.problemId;//保存当前题目的题目编号

    updatedProgress.sourcePath=filePath.toStdString();//保存用户选择的原始源码路径

    updatedProgress.lastOpenedTime=QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss").toStdString();//更新最近打开的时间

    std::string saveError;// 用于保存 进度保存错误

    // 尝试保存进度
    if(!progressStore_->saveProgress(updatedProgress,saveError))
    {
        ui->statusLabel->setText(QString::fromUtf8(saveError.c_str()));

        return;
    }

    currentProgress_=updatedProgress;// 保存成功后更新内存中的进度

    sourcePath_=filePath; //更新当前源码路径

    ui->sourcePathEdit->setText(sourcePath_); //显示保存结果

    ui->judgeButton->setEnabled(hasSelectedProblem_);//已选择题目时才允许判题
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

    const QFileInfo sourceInfo(sourcePath_); // 创建源码文件信息对象

    if(!sourceInfo.exists() || !sourceInfo.isFile()) // 检查源码是否仍然存在且为文件
    {
        ui->statusLabel->setText(QStringLiteral("源码路径已失效，请重新选择")); // 显示路径失效提示

        ui->judgeButton->setEnabled(false); // 路径失效时禁止判题

        return; // 路径无效时结束判题函数
    }

    ui->countLabel->setText( // 清除上一次统计结果
        QStringLiteral("通过 0，已执行 0，总点数 0")); // 显示初始统计

    ui->pointResultTable->setRowCount(0); // 清除上一次的逐点结果

    ui->actualOutputText->clear(); // 清除上一次的实际输出

    ui->expectedOutputText->clear(); // 清除上一次的标准输出

    ui->errorOutputText->clear(); // 清除上一次的错误信息

    std::string error; // 保存测试点读取错误

    std::vector<TestCase> tests; // 保存当前题目的全部测试点

    if (currentProblem_.testCount < 1 || // 检查测试点数量下限
        currentProblem_.testCount > 20) // 检查测试点数量上限
    {
        ui->statusLabel->setText( // 显示题目数据错误
            QStringLiteral("题目测试点数量无效")); // 设置错误提示

        return; // 数据无效时结束判题
    }

    for (int testIndex = 1; testIndex <= currentProblem_.testCount; ++testIndex)
    {
        TestCase testCase; // 保存当前测试点

        // 保存当前测试点读取结果
        const bool testLoaded =library_->getTestCase( currentProblem_.packageId, currentProblem_.problemId, testIndex,testCase, error);

        if (!testLoaded) // 判断当前测试点是否读取失败
        {
            ui->statusLabel->setText( QString::fromUtf8( error.c_str()));
            return; // 任一点读取失败时不启动判题
        }

        tests.push_back(testCase); // 将当前测试点加入完整列表
    }

    const bool accepted=judgeEngine_->startJudge(currentProblem_,sourcePath_.toStdString(),tests,error);

    if (!accepted) // 请求未被接受，没有启动判题任务
    {
        const QString errorText = QString::fromUtf8(error.c_str());

        ui->resultLabel->setText(QStringLiteral("判题未启动"));
        ui->errorOutputText->setPlainText(errorText); // 在右侧显示具体原因
        ui->statusLabel->setText(errorText);

        return; // 保留旧进度，不进入任务执行流程
    }

    ui->resultLabel->setText(QStringLiteral("判题中")); // 请求被接受后才显示

    //禁止判题期间重复判题、重新选择源码、导入题包、刷新题库、检查编译器、切换题目、移除题包
    ui->judgeButton->setEnabled(false);
    ui->chooseSourceButton->setEnabled(false);
    ui->importButton->setEnabled(false);
    ui->refreshButton->setEnabled(false);
    ui->checkCompilerButton->setEnabled(false);
    ui->problemTable->setEnabled(false);
    ui->removePackageButton->setEnabled(false);
}

void MainWindow::onJudgeFinished()
{
    const JudgeResult result=judgeEngine_->getResult();

    ui->countLabel->setText(QStringLiteral("通过%1，已执行%2，总点数%3").arg(result.passedCount).arg(result.executedCount).arg(result.totalCount));//更新测试点统计文字

    ui->pointResultTable->setRowCount(static_cast<int>(result.pointResults.size()));//设置结果表的行数

    for (int row = 0; row < static_cast<int>(result.pointResults.size()); ++row) {
        const TestPointResult& point=result.pointResults[static_cast<std::size_t>(row)];//获取当前测试点结果
        auto* indexItem=new QTableWidgetItem(QString::number(point.index));//创建测试点编号单元格
        auto* statusItem=new QTableWidgetItem(QString::fromUtf8(point.status.c_str()));//创建状态单元格
        auto* elapsedItem=new QTableWidgetItem(QStringLiteral("%1ms").arg(point.elapsedMs));//创建耗时单元格

        QString message;//保存当前测试点的说明文字

        if(!point.error.empty())//判断是否存在工具或文件错误
        {
            message=QString::fromUtf8(point.error.c_str());
        }
        else if (!point.runtimeError.empty()) // 判断是否存在运行错误
        {
            message = QString::fromUtf8(
                point.runtimeError.c_str());
        }
        else if (!point.executed) // 判断测试点是否没有执行
        {
            message = QStringLiteral("未执行");
        }
        else if (point.status == "AC") // 判断当前测试点是否通过
        {
            message = QStringLiteral("通过");
        }
        else // 处理 WA 等其他状态
        {
            message = QStringLiteral("输出与标准答案不一致");
        }

        auto*messageItem=new QTableWidgetItem(message);//创建说明格单元

        //写入测试点编号、状态、耗时、说明
        ui->pointResultTable->setItem(row,0,indexItem);
        ui->pointResultTable->setItem(row,1,statusItem);
        ui->pointResultTable->setItem(row,2,elapsedItem);
        ui->pointResultTable->setItem(row,3,messageItem);

    }

    Progress updatedProgress=currentProgress_;//创建本次判题后的候选进度

    updatedProgress.userId=userId_.toStdString(); //保存当前用户ID

    updatedProgress.packageId=currentProblem_.packageId;//保存题包编号

    updatedProgress.problemId=currentProblem_.problemId;//保存题目编号

    updatedProgress.sourcePath=sourcePath_.toStdString();//保存用户源码路径

    if(result.status=="AC")//判断测试点是否全部通过
    {
        updatedProgress.status=2;
    }

    //判断答案错误、编译错误、运行错误、运行超时、输出超限
    else if(result.status=="WA"||result.status=="CE"||result.status=="RE"||result.status=="TLE"||result.status=="OutputLimit")
    {
        updatedProgress.status=1;
    }

    //生成最近一次判题摘要
    updatedProgress.lastSummary=result.status+"，测试点"+std::to_string(result.testIndex);

    //保存最近判题时间
    updatedProgress.lastJudgedTime=QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss").toStdString();

    std::string saveError;//保存：进度保存错误

    const bool shouldSave =
        result.status == "AC" ||
        result.status == "WA" ||
        result.status == "CE" ||
        result.status == "RE" ||
        result.status == "TLE" ||
        result.status == "OutputLimit"; // 只有有效判题结果才更新练习记录

    const bool saved =
        shouldSave &&
        progressStore_->saveProgress(updatedProgress, saveError); // 系统错误时跳过保存

    if (saved)
    {
        currentProgress_ = updatedProgress; // 只有保存成功才更新当前内存记录
    }

    ui->resultLabel->setText(QString::fromUtf8(result.status.c_str()));

    const std::string outputPreview =
        result.actualOutput.substr(0, 4096); // 最多取前 4096 字节供界面显示

    QString outputText =
        QString::fromUtf8(outputPreview.c_str()); // 转为界面文字

    if (result.actualOutput.size() > 4096)
    {
        outputText += QStringLiteral(
            "\n\n[输出过长，界面仅显示前 4096 字节]");
    }

    ui->actualOutputText->setPlainText(outputText); // 显示有限长度的预览

    ui->expectedOutputText->setPlainText(QString::fromUtf8(result.expectedOutput.c_str()));

    QString errorText=QString::fromUtf8(result.compilerOutput.c_str());

    if(errorText.isEmpty())
    {
        errorText=QString::fromUtf8(result.error.c_str());
    }

    ui->errorOutputText->setPlainText(errorText);

    if (!shouldSave)
    {
        ui->statusLabel->setText(
            QStringLiteral("本次发生系统错误，保留原有效进度"));
    }
    else if (saved) // 有效结果保存成功
    {
        ui->statusLabel->setText( // 显示判题和保存结果
            QStringLiteral("判题完成，耗时 %1 ms，进度已保存") // 设置提示格式
                .arg(result.elapsedMs)); // 填入耗时
    }
    else // 进度保存失败时进入此分支
    {
        ui->errorOutputText->append(
            QStringLiteral("进度保存失败：") +
            QString::fromUtf8(saveError.c_str())); // 在右侧同时显示保存错误
            ui->statusLabel->setText( // 显示判题完成和保存失败
            QStringLiteral("判题完成，耗时 %1 ms，进度保存失败：%2") // 设置提示格式
                .arg(result.elapsedMs) // 填入耗时
                .arg(QString::fromUtf8(saveError.c_str()))); // 填入保存错误
    }
    // 恢复判题、源码选择、导入、刷新、编译器检查、切换题目、移除题包按钮
    ui->judgeButton->setEnabled(true);
    ui->chooseSourceButton->setEnabled(true);
    ui->importButton->setEnabled(true);
    ui->refreshButton->setEnabled(true);
    ui->checkCompilerButton->setEnabled(true);
    ui->problemTable->setEnabled(true);
    ui->removePackageButton->setEnabled(hasSelectedProblem_);

}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (judgeEngine_->isBusy())
    {
        ui->statusLabel->setText(QStringLiteral("判题进行中，请等待结束后再关闭"));
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::clearSource()//函数作用：清除源码绑定
{
    if(!hasSelectedProblem_)//判断当前是否已经选择题目
    {
        ui->statusLabel->setText(QStringLiteral("请先选择题目"));

        return;
    }

    Progress updatedProgress=currentProgress_;//创建新的候选进度

    // 保存当前用户 ID、题包编号、题目编号
    updatedProgress.userId = userId_.toStdString();

    updatedProgress.packageId = currentProblem_.packageId;

    updatedProgress.problemId = currentProblem_.problemId;

    updatedProgress.sourcePath.clear(); // 清空保存的源码路径

    std::string saveError; // 保存进度错误信息

    const bool saved=progressStore_->saveProgress(updatedProgress,saveError);//保存清除后的进度

    if(!saved)
    {
        ui->statusLabel->setText(QString::fromUtf8(saveError.c_str()));
        return;
    }

    currentProgress_ = updatedProgress; // 保存成功后更新当前进度

    sourcePath_.clear(); // 清空窗口中的源码路径

    ui->sourcePathEdit->clear(); // 清空路径输入框

    ui->judgeButton->setEnabled(false); // 没有源码时禁用判题按钮

    ui->statusLabel->setText( QStringLiteral("源码绑定已清除")); // 显示清除成功
}

// 定义登录流程
bool MainWindow::showLoginDialog()
{
    // 创建登录对话框
    LoginDialog dialog(this);

    // 显示对话框并等待用户操作
    if (dialog.exec() != QDialog::Accepted)
    {
        // 用户取消时返回失败
        return false;
    }

    // 读取用户输入的用户名
    const QString inputUserId =dialog.userId();

    // 读取用户输入的密码
    const QString inputPassword =dialog.password();

    // 读取用户是否点击了注册按钮
    const bool registerRequested =dialog.property("registerRequested").toBool();

    // 保存账号操作错误
    std::string error;

    // 判断当前是否执行注册操作
    if (registerRequested)
    {
        // 调用账号注册接口
        const bool registered =accountManager_->registerAccount(inputUserId.toStdString(),inputPassword.toStdString(),error);

        // 判断注册是否失败
        if (!registered)
        {
            // 显示注册失败提示
            QMessageBox::warning(this,QStringLiteral("注册失败"),QString::fromUtf8(error.c_str()));

            // 注册失败后重新显示登录窗口
            return showLoginDialog();
        }

        // 显示注册成功提示
        QMessageBox::information(this,QStringLiteral("注册成功"),QStringLiteral("账号注册成功，请重新登录"));

        // 注册成功后重新进入登录流程
        return showLoginDialog();
    }

    // 调用账号登录接口
    const bool loggedIn =accountManager_->login(inputUserId.toStdString(),inputPassword.toStdString(),error);

    // 判断登录是否失败
    if (!loggedIn)
    {
        // 显示登录失败提示
        QMessageBox::warning(this,QStringLiteral("登录失败"),QString::fromUtf8(error.c_str()));

        // 登录失败后重新显示登录窗口
        return showLoginDialog();
    }

    // 保存当前登录用户名
    currentUserId_ =inputUserId;
    userId_ =inputUserId;

    // 返回登录成功
    return true;
}

// 定义退出账号函数
void MainWindow::logoutAccount()
{
    // 判题期间不允许退出账号
    if (judgeEngine_->isBusy())
    {
        ui->statusLabel->setText(QStringLiteral("判题进行中，暂时不能退出账号"));

        return;
    }

    // 清空当前选中的题目
    currentProblem_ = Problem();

    // 清空当前题目的进度
    currentProgress_ = Progress();

    // 清空源码路径
    sourcePath_.clear();

    // 标记当前没有选中题目
    hasSelectedProblem_ = false;

    // 退出账号后禁用移除题包按钮
    ui->removePackageButton->setEnabled(false);

    // 退出账号管理器中的当前登录状态
    accountManager_->logout();

    // 清空主窗口中的当前用户
    currentUserId_.clear();

    // 清空进度查询使用的用户
    userId_.clear();

    // 清空源码路径输入框
    ui->sourcePathEdit->clear();

    // 清空题面显示区域
    ui->statementText->clear();

    // 清空实际输出
    ui->actualOutputText->clear();

    // 清空标准输出
    ui->expectedOutputText->clear();

    // 清空错误输出
    ui->errorOutputText->clear();

    // 清空测试点结果表
    ui->pointResultTable->setRowCount(0);

    // 重置测试点统计
    ui->countLabel->setText(QStringLiteral("通过 0，已执行 0，总点数 0"));

    // 重置判题状态
    ui->resultLabel->setText(QStringLiteral("等待判题"));

    // 禁用开始判题按钮
    ui->judgeButton->setEnabled(false);

    // 再次显示登录窗口
    if (!showLoginDialog())
    {
        // 用户取消登录时关闭主窗口
        close();
        return;
    }

    // 登录新账号后刷新题目状态
    refreshProblems();

    // 显示新账号登录成功
    ui->statusLabel->setText(QStringLiteral("已切换账号"));
}

// 打开筛选对话框
void MainWindow::openFilterDialog()
{
    // 创建筛选对话框
    FilterDialog dialog(this);

    // 获取当前题库中的全部题目
    const std::vector<Problem> problems =library_->getProblems();

    // 创建标签列表
    QStringList tags;

    // 创建题包列表
    QStringList packages;

    // 遍历当前题库中的全部题目
    for (const Problem& problem : problems)
    {
        // 添加当前题包编号
        if (!packages.contains(QString::fromUtf8(problem.packageId.c_str())))
        {
            // 将题包编号加入下拉框数据
            packages.append(QString::fromUtf8(problem.packageId.c_str()));
        }

        // 遍历当前题目的标签
        for (const std::string& tag : problem.tags)
        {
            // 将标签转换为 QString
            const QString tagText =QString::fromUtf8(tag.c_str());

            // 只添加尚未出现过的标签
            if (!tags.contains(tagText))
            {
                // 将标签加入下拉框数据
                tags.append(tagText);
            }
        }
    }

    // 将标签列表交给筛选对话框
    dialog.setTags(tags);

    // 将题包列表交给筛选对话框
    dialog.setPackages(packages);

    // 显示筛选对话框并等待用户操作
    if (dialog.exec() != QDialog::Accepted)
    {
        // 用户取消筛选时直接返回
        return;
    }

    // 保存标题筛选条件
    titleFilter_ =dialog.titleText();

    // 保存标签筛选条件
    tagFilter_ =dialog.selectedTag();

    // 保存状态筛选条件
    statusFilter_ =dialog.selectedStatus();

    // 保存题包筛选条件
    packageFilter_ =dialog.selectedPackage();

    // 根据新的筛选条件刷新题目列表
    refreshProblems();
}

// 判断题目是否符合当前筛选条件
bool MainWindow::matchesFilters(const Problem& problem) const
{
    // 判断标题筛选条件是否匹配
    if (!titleFilter_.isEmpty() &&!titleFilter_.contains(QString::fromUtf8(problem.title.c_str()),Qt::CaseInsensitive))
    {
        // 当前题目标题不符合条件
        return false;
    }

    // 判断题包筛选条件是否匹配
    if (packageFilter_ != QStringLiteral("全部题包") &&packageFilter_ != QString::fromUtf8(problem.packageId.c_str()))
    {
        // 当前题包不符合条件
        return false;
    }

    // 判断标签筛选条件是否匹配
    if (tagFilter_ != QStringLiteral("全部标签"))
    {
        // 保存标签是否匹配
        bool tagMatched = false;

        // 遍历当前题目的全部标签
        for (const std::string& tag : problem.tags)
        {
            // 判断当前标签是否与筛选条件相同
            if (tagFilter_ ==QString::fromUtf8(tag.c_str()))
            {
                // 标记标签匹配成功
                tagMatched = true;

                // 停止继续检查标签
                break;
            }
        }

        // 判断是否没有匹配标签
        if (!tagMatched)
        {
            // 当前题目不符合标签条件
            return false;
        }
    }

    // 判断状态筛选条件是否需要检查
    if (statusFilter_ != QStringLiteral("全部状态"))
    {
        // 创建当前题目的进度对象
        Progress progress;

        // 保存读取进度时的错误
        std::string error;

        // 读取当前用户的题目进度
        const bool loaded =progressStore_->getProgress(userId_.toStdString(),problem.packageId,problem.problemId,progress,error);

        // 进度读取失败时视为不匹配
        if (!loaded)
        {
            // 当前题目无法取得状态
            return false;
        }

        // 保存当前题目的状态文字
        QString currentStatus;

        // 判断是否为本地通过
        if (progress.status == 2)
        {
            // 设置本地通过状态
            currentStatus =QStringLiteral("本地通过");
        }
        else if (progress.status == 1)
        {
            // 设置练习中状态
            currentStatus =QStringLiteral("练习中");
        }
        else
        {
            // 设置未开始状态
            currentStatus = QStringLiteral("未开始");
        }

        // 判断状态是否匹配
        if (currentStatus != statusFilter_)
        {
            // 当前题目不符合状态条件
            return false;
        }
    }

    return true;
}

void MainWindow::removeSelectedPackage()
{
    // 判断是否已经选择题目
    if (!hasSelectedProblem_)
    {
        // 显示提示
        ui->statusLabel->setText(QStringLiteral("请先选择题目"));
        return;
    }

    // 判断当前题包编号是否为空
    if (selectedPackageId_.isEmpty())
    {
        // 显示提示
        ui->statusLabel->setText(QStringLiteral("当前没有可移除的题包"));
        return;
    }

    // 弹出移除确认窗口
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        QStringLiteral("确认移除题包"),
        QStringLiteral("移除后题包会从共享题库隐藏，缓存和进度会保留。是否继续？"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    // 判断用户是否确认移除
    if (answer != QMessageBox::Yes)
    {
        return;
    }

    // 保存本次运行中隐藏的题包
    removedPackageIds_.insert(selectedPackageId_);

    // 将隐藏题包编号写入文件，保证重启后仍然隐藏
    if (!saveRemovedPackages(removedPackagesPath_))
    {
        ui->statusLabel->setText(QStringLiteral("无法保存题包隐藏状态"));
        return;
    }
    // 清空当前题目
    currentProblem_ = Problem();
    // 清空当前进度
    currentProgress_ = Progress();
    // 清空当前题包编号
    selectedPackageId_.clear();
    // 标记当前没有选中题目
    hasSelectedProblem_ = false;
    // 清空源码路径
    sourcePath_.clear();
    // 清空题面
    ui->statementText->clear();
    // 清空源码路径框
    ui->sourcePathEdit->clear();
    // 禁用判题按钮
    ui->judgeButton->setEnabled(false);
    // 移除题包后禁用移除按钮
    ui->removePackageButton->setEnabled(false);
    // 刷新题目列表
    refreshProblems();
    // 显示操作结果
    ui->statusLabel->setText(QStringLiteral("题包已隐藏"));
}













