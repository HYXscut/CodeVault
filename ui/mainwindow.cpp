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
#include <QTableWidgetItem>
#include <QFileDialog>
#include <QFileInfo>
#include <QCloseEvent>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QAbstractItemView>
#include <QHeaderView>

// 构造主窗口对象
// 功能说明：初始化主界面、表格与布局，实例化各核心业务模块，完成账号登录与题库及进度初始化
// 参数说明：parent 父窗口指针，默认为 nullptr
// 实现步骤：1. setupUi 初始化控件 2. 配置表格表头与列宽 3. 构建各业务管理类对象 4. 加载账号文件并弹出登录对话框 5. 加载进度文件与题库 6. 连接所有信号槽 7. 刷新题目列表
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
    //进行一些格式和功能的设置
    ui->problemTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn); // 底部始终保留横向滚动条。
    ui->problemTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel); // 按像素平滑滚动。
    ui->problemTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive); // 用户仍可拖动表头调整列宽。
    ui->problemTable->horizontalHeader()->setStretchLastSection(false); // 保留实际列宽，超出视口时由滚动条访问。
    ui->problemTable->horizontalHeader()->setMinimumSectionSize(70);
    ui->problemTable->setColumnWidth(0, 110); // 题包列。
    ui->problemTable->setColumnWidth(1, 80); // 题号列。
    ui->problemTable->setColumnWidth(2, 220); // 标题列预留更多空间。
    ui->problemTable->setColumnWidth(3, 110); // 状态列。
    ui->problemTable->verticalHeader()->hide(); // 隐藏题目列表左侧的行号表头
    ui->pointResultTable->verticalHeader()->hide(); // 隐藏逐点结果表左侧行号和左上角黑块
    ui->pointResultTable->setColumnCount(4);//设置测试点结果表的列数
    ui->pointResultTable->setHorizontalHeaderLabels({QStringLiteral("测试点"),QStringLiteral("状态"),QStringLiteral("耗时"),QStringLiteral("说明")});//设置表头文字
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
        QMessageBox::critical(this,QStringLiteral("账号文件错误"),QString::fromUtf8(accountError.c_str()));
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
        QMessageBox::critical(this,QStringLiteral("进度文件加载失败"),QString::fromUtf8(progressError.c_str()) +QStringLiteral("\n原文件未修改。请检查文件后重新启动。"));

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

    std::string libraryError;
    if (!library_->load(libraryError))
    {
        QMessageBox::critical(this, QStringLiteral("题库加载失败"), QString::fromUtf8(libraryError.c_str()));
        return;
    }
    refreshProblems();

    startupReady_ = true; // 登录成功并完成主窗口初始化
}

// 析构主窗口对象
// 功能说明：释放 Qt Designer 动态分配的界面资源
// 参数说明：无
MainWindow::~MainWindow()
{
    delete ui;
}

// 查询启动就绪状态
// 功能说明：查询主窗口是否已经成功完成启动初始化与账号登录
// 参数说明：无
// 返回值：启动且登录成功返回 true，否则返回 false
// 实现步骤：直接返回 startupReady_ 成员变量
bool MainWindow::isStartupReady() const
{
    return startupReady_; // 返回启动状态
}

// 刷新题目列表
// 功能说明：根据当前的标题关键字、标签、状态及题包筛选条件，查询题库并刷新题目列表展示
// 参数说明：无
// 实现步骤：1. 检查判题引擎忙碌状态 2. 加载题库并构建 ProblemQuery 3. 调用 searchProblems 获取匹配题目 4. 逐行填充表格并更新各题目练习状态
void MainWindow::refreshProblems()//函数作用：刷新题目列表
{
    if (judgeEngine_->isBusy()) return; // 判题中禁止修改当前绑定及题库。

    std::string error;//保存题库加载失败时的错误信息

    if(!library_->load(error))//尝试加载已保存的题目
    {
        ui->problemTable->setRowCount(0);//加载失败时清空题目表格

        ui->statusLabel->setText(//设置状态栏中的错误文字
            QString::fromUtf8(error.c_str()) //将UTF8转换为QString
            );

        return;
    }

    ProblemQuery query; // 窗口只转换控件条件，筛选规则统一交给 A 的题库模块。
    query.titleKeyword = titleFilter_.toUtf8().toStdString();
    if (tagFilter_ != QStringLiteral("全部标签")) query.tag = tagFilter_.toUtf8().toStdString();
    if (packageFilter_ != QStringLiteral("全部题包")) query.packageId = packageFilter_.toUtf8().toStdString();
    if (statusFilter_ == QStringLiteral("未开始")) query.status = ProgressNotStarted;
    if (statusFilter_ == QStringLiteral("练习中")) query.status = ProgressInProgress;
    if (statusFilter_ == QStringLiteral("本地通过")) query.status = ProgressLocalAccepted;
    const std::vector<Problem> problems = library_->searchProblems(
        query, progressStore_->getUserProgress(userId_.toUtf8().toStdString()));

    ui->problemTable->setRowCount(0); // 清空旧的显示结果

    int displayRow = 0; // 保存当前显示行号

    for (const Problem& problem : problems) // 遍历全部题目
    {
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

        auto* statusItem = new QTableWidgetItem(progressText);
        packageItem->setData(Qt::UserRole, QString::fromUtf8(problem.packageId.c_str()));
        packageItem->setData(Qt::UserRole + 1, QString::fromUtf8(problem.problemId.c_str()));
        ui->problemTable->setItem(displayRow, 0, packageItem);
        ui->problemTable->setItem(displayRow, 1, problemItem);
        ui->problemTable->setItem(displayRow, 2, titleItem);
        ui->problemTable->setItem(displayRow, 3, statusItem);
        ++displayRow;
    }

    ui->statusLabel->setText( // 更新题目数量提示
        QStringLiteral("显示 %1 道题目").arg(displayRow)); // 显示筛选后的数量
}

// 导入外部题包
// 功能说明：打开目录选择对话框，选择题包文件夹并导入到共享题库
// 参数说明：无
// 实现步骤：1. 检查判题引擎状态 2. 弹出文件夹选择对话框 3. 调用 library_->importPackage 导入题包 4. 导入成功则刷新题目列表
void MainWindow::importPackage()//函数作用：导入题包
{
    if (judgeEngine_->isBusy()) return; // 判题中禁止更改共享题库和筛选。

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
        const QString reason = QString::fromUtf8(error.c_str());
        ui->statusLabel->setText(reason); // 底部保留原因，关闭弹窗后仍可查看。
        QMessageBox::warning(this, QStringLiteral("导入失败"), reason); // 明确显示拒绝原因及具体文件，避免状态栏不易察觉。
        return;
    }

    refreshProblems();

    ui->statusLabel->setText(QStringLiteral("题包导入成功"));
}

// 展示选中题目详情
// 功能说明：响应题目列表行点击事件，获取题目详情展示题面并恢复该题的源码绑定与做题进度
// 参数说明：row 选中的表格行；column 选中的表格列（忽略）
// 实现步骤：1. 从选中行的 UserRole 中提取题包与题目编号 2. 调用 findProblem 获取题目详情 3. 读取该题目在当前用户下的进度 4. 更新源码路径与题面展示
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
        ui->statusLabel->setText(QStringLiteral("%1 的最近结果：%2").arg(currentUserId_, QString::fromUtf8(currentProgress_.lastSummary.c_str())));
    }

        ui->judgeButton->setEnabled(
        !sourcePath_.isEmpty() && QFileInfo(sourcePath_).isFile()); // 路径非空且指向普通文件才允许判题

    //在题面区域显示题面
    ui->statementText->setPlainText(QString::fromUtf8(problem.statement.c_str()));
    
}

// 检查编译器环境
// 功能说明：发起本地 GCC 编译器工具链环境异步检查
// 参数说明：无
// 实现步骤：1. 调用 checker_->checkCompiler() 触发异步检测 2. 禁用检查按钮并更新状态提示
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

// 编译器检查完成回调
// 功能说明：接收编译器异步检测完成信号，读取检查结果并更新界面状态与按钮可用性
// 参数说明：无
// 实现步骤：1. 调用 getCompilerInfo 获取结果 2. 根据 found 状态显示编译器版本或错误 3. 重新启用检查按钮
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

// 选择本地源码文件
// 功能说明：打开文件选择对话框，选择本地 C++ 源码并与当前题目完成绑定及进度保存
// 参数说明：无
// 实现步骤：1. 检查判题状态 2. 打开对话框选择 .cpp 文件 3. 校验路径有效性 4. 组装 Progress 并调用 saveProgress 保存 5. 更新界面控件
void MainWindow::chooseSource()
{
    if (judgeEngine_->isBusy()) return; // 判题中禁止修改当前绑定及题库。

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

// 发起判题评测
// 功能说明：锁定当前题目身份并启动判题任务，包括测试点加载、环境验证与判题引擎触发
// 参数说明：无
// 实现步骤：1. 校验账号身份一致性 2. 检查题目选择与源码文件存在性 3. 循环加载测试点数据 4. 锁定 judgingProgress_ 5. 调用 startJudge 触发评测 6. 禁用冲突按钮
void MainWindow::startJudge()
{
    const std::string sessionId = accountManager_->currentUserId();
    if (!accountManager_->isLoggedIn() || userId_.toUtf8().toStdString() != sessionId ||
        currentProgress_.userId != sessionId)
    {
        ui->resultLabel->setText(QStringLiteral("账号身份不一致，未启动判题"));
        ui->statusLabel->setText(QStringLiteral("请退出后重新登录并选择题目；本次没有修改进度"));
        return; // 账号、窗口和所选记录必须属于同一身份，不能把结果写给其他用户。
    }

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

    judgingProgress_ = currentProgress_; // 保存任务身份，完成回调不依赖可变的控件。
    judgingProgress_.userId = userId_.toUtf8().toStdString();
    judgingProgress_.packageId = currentProblem_.packageId;
    judgingProgress_.problemId = currentProblem_.problemId;
    judgingProgress_.sourcePath = sourcePath_.toUtf8().toStdString();

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
    ui->clearSourceButton->setEnabled(false);
    ui->logoutButton->setEnabled(false);
    ui->filterButton->setEnabled(false);
}

// 判题完成处理槽函数
// 功能说明：接收判题完成信号，展示逐点结果、耗时、实际输出与错误日志，并更新保存有效进度
// 参数说明：无
// 实现步骤：1. 获取 JudgeResult 2. 填充逐点测试结果表 3. 根据最终状态更新 Progress 4. 调用 saveProgress 保存 5. 更新输出展示 6. 恢复操作按钮并刷新列表
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

    Progress updatedProgress=judgingProgress_; // 使用发起任务时锁定的账号、题目和源码副本。

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

    ui->resultLabel->setText(QStringLiteral("%1：%2").arg(currentUserId_, QString::fromUtf8(result.status.c_str()))); // 显示结果所属账号。

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
    ui->clearSourceButton->setEnabled(true);
    ui->logoutButton->setEnabled(true);
    ui->filterButton->setEnabled(true);
    const QString completionMessage = ui->statusLabel->text();
    refreshProblems(); // 保存后立即更新列表中的当前账号状态及筛选结果。
    ui->statusLabel->setText(completionMessage);

}

// 窗口关闭事件处理
// 功能说明：拦截窗口关闭请求，若处于判题运行状态则阻止退出，保障数据安全
// 参数说明：event 窗口关闭事件指针
// 实现步骤：1. 判断判题引擎是否忙碌 2. 忙碌则忽略事件并提示 3. 空闲则调用基类关闭事件
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

// 清除源码绑定
// 功能说明：解除当前题目的本地 C++ 源码路径绑定并同步保存进度
// 参数说明：无
// 实现步骤：1. 检查判题状态与题目选中状态 2. 清空 Progress 中的 sourcePath 并调用 saveProgress 3. 清空界面路径显示并禁用判题按钮
void MainWindow::clearSource()//函数作用：清除源码绑定
{
    if (judgeEngine_->isBusy()) return; // 判题中禁止修改当前绑定及题库。

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
// 功能说明：循环显示模态登录窗口，处理用户登录认证与账号注册，绑定当前会话身份
// 参数说明：无
// 返回值：登录成功返回 true，用户取消或关闭登录返回 false
// 实现步骤：1. 循环构造 LoginDialog 2. 若用户请求注册则调用 registerAccount 3. 若用户请求登录则调用 login 4. 登录成功绑定 currentUserId_ 与 userId_ 5. 设置主界面标题
bool MainWindow::showLoginDialog()
{
    while (true) // 失败后重新显示窗口，用循环代替递归，避免多次输错不断增加调用层数。
    {
        LoginDialog dialog(this);
        if (dialog.exec() != QDialog::Accepted) return false;
        const QString username = dialog.userId();
        const std::string password = dialog.password().toUtf8().toStdString();
        std::string error;
        if (dialog.property("registerRequested").toBool())
        {
            if (!accountManager_->registerAccount(username.toUtf8().toStdString(), password, error))
                QMessageBox::warning(this, QStringLiteral("注册失败"), QString::fromUtf8(error.c_str()));
            else
                QMessageBox::information(this, QStringLiteral("注册成功"), QStringLiteral("账号已保存，请登录"));
            continue;
        }
        std::string stableId;
        if (!accountManager_->login(username.toUtf8().toStdString(), password, stableId, error))
        {
            QMessageBox::warning(this, QStringLiteral("登录失败"), QString::fromUtf8(error.c_str()));
            continue;
        }
        currentUserId_ = username; // 用户名用于显示。
        userId_ = QString::fromUtf8(stableId.c_str()); // 内部 ID 用于进度读取和任务归属。
        setWindowTitle(QStringLiteral("CodeVault — %1").arg(username));
        ui->brandTitle->setText(QStringLiteral("CodeVault · 当前账号：%1").arg(username)); // 主界面正文也显示身份。
        return true;
    }
}

// 定义退出账号函数
// 功能说明：注销当前登录会话，清空所有界面数据与题目状态，并重新弹出登录对话框
// 参数说明：无
// 实现步骤：1. 检查判题忙碌状态 2. 清空题目、进度、源码与各筛选条件 3. 调用 accountManager_->logout 4. 清空界面所有展示控件 5. 重新调用 showLoginDialog 6. 刷新题库
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
    judgingProgress_ = Progress(); // 清空上一账号的任务身份。
    selectedPackageId_.clear();
    ui->problemTable->setRowCount(0); // 登录窗口显示期间也不保留上一账号的列表状态。
    titleFilter_.clear();
    tagFilter_ = QStringLiteral("全部标签");
    statusFilter_ = QStringLiteral("全部状态");
    packageFilter_ = QStringLiteral("全部题包");
    setWindowTitle(QStringLiteral("CodeVault"));
    ui->brandTitle->setText(QStringLiteral("CodeVault · 未登录"));

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
    ui->statusLabel->setText(QStringLiteral("已切换账号：%1").arg(currentUserId_));
}

// 打开筛选对话框
// 功能说明：收集题库中已有的全部标签与题包，弹出多条件筛选对话框并刷新列表
// 参数说明：无
// 实现步骤：1. 检查判题状态 2. 遍历题库提取去重的题包与标签 3. 设置给 FilterDialog 并模态执行 4. 若确认则保存筛选条件并调用 refreshProblems 刷新
void MainWindow::openFilterDialog()
{
    if (judgeEngine_->isBusy()) return; // 判题中禁止更改共享题库和筛选。

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

// 移除选中的题包
// 功能说明：向用户弹窗确认后，从共享题库中隐藏当前选中的题包并重置当前题目状态
// 参数说明：无
// 实现步骤：1. 校验题目与题包选中状态 2. 弹出 QMessageBox 确认 3. 调用 removePackageFromLibrary 隐藏题包 4. 清空当前题面、进度与输出 5. 刷新题目列表
void MainWindow::removeSelectedPackage()
{
    if (judgeEngine_->isBusy()) return; // 判题中禁止更改共享题库和筛选。

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

    std::string error;
    if (!library_->removePackageFromLibrary(selectedPackageId_.toUtf8().toStdString(), error))
    {
        ui->statusLabel->setText(QString::fromUtf8(error.c_str()));
        return; // 保存失败时保留题面、选择与原可见性。
    }
    ui->resultLabel->clear();
    ui->actualOutputText->clear();
    ui->expectedOutputText->clear();
    ui->errorOutputText->clear();
    ui->pointResultTable->setRowCount(0);
    ui->countLabel->setText(QStringLiteral("通过0，已执行0，总点数0"));
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













