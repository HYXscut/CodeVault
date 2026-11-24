#include "logindialog.h" // 引入登录对话框类的声明
#include "ui_logindialog.h" // 引入 Qt Designer 生成的界面类

#include <QPushButton> // 使用按钮控件类型

// 创建登录对话框
// 功能说明：初始化对话框界面组件与显示属性，建立按钮点击信号与处理槽函数的连接
// 参数说明：parent 父窗口指针，默认为 nullptr
// 实现步骤：1. 根据 .ui 文件创建界面控件 2. 设置无边框与背景属性 3. 密码输入框设为密文模式并聚焦用户名 4. 连接各按钮的点击信号至对应槽函数
LoginDialog::LoginDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoginDialog) // 创建界面对象
{
    ui->setupUi(this); // 根据 .ui 文件创建界面控件

    // 设置无边框窗口和窗口背景属性
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);

    // 将密码输入框设置为隐藏字符显示
    ui->passwordEdit->setEchoMode(QLineEdit::Password);

    // 窗口打开后自动聚焦用户名输入框
    ui->usernameEdit->setFocus();

    // 连接登录按钮的点击信号
    connect(ui->loginButton, &QPushButton::clicked,this, &LoginDialog::onLoginClicked);

    connect(ui->registerButton, &QPushButton::clicked,this, &LoginDialog::onRegisterClicked);

    connect(ui->cancelButton, &QPushButton::clicked,this, &LoginDialog::onCancelClicked);

    connect(ui->closeButton, &QPushButton::clicked,this, &LoginDialog::onCancelClicked);
}

// 析构登录对话框对象
// 功能说明：释放 Qt Designer 动态分配的界面对象资源
// 参数说明：无
LoginDialog::~LoginDialog()
{
    delete ui; // 释放界面对象
}

// 返回用户输入的用户名
// 功能说明：获取用户在用户名输入框中输入的文本并去除首尾空格
// 参数说明：无
// 返回值：去除首尾空格后的用户名字符串
// 实现步骤：1. 读取 usernameEdit 的文本 2. 调用 trimmed 去除空格并返回
QString LoginDialog::userId() const
{
    return ui->usernameEdit->text().trimmed(); // 去除用户名首尾空格
}

// 返回用户输入的密码
// 功能说明：获取用户在密码输入框中输入的原始密码文本
// 参数说明：无
// 返回值：密码输入框中的字符串
// 实现步骤：直接读取并返回 passwordEdit 的文本内容
QString LoginDialog::password() const
{
    return ui->passwordEdit->text(); // 读取密码内容
}

// 处理登录按钮点击
// 功能说明：校验用户输入的完整性，若非空则关闭对话框并返回 Accepted 通知主窗口登录
// 参数说明：无
// 实现步骤：1. 检查用户名是否为空，为空则提示并聚焦 2. 检查密码是否为空，为空则提示并聚焦 3. 输入完整后关闭对话框并返回 Accepted
void LoginDialog::onLoginClicked()
{
    // 检查用户名是否为空
    if (userId().isEmpty())
    {
        ui->messageLabel->setText(QStringLiteral("用户名不能为空")); // 显示错误提示
        ui->usernameEdit->setFocus(); // 将焦点返回用户名输入框
        return; // 停止本次登录
    }

    // 检查密码是否为空
    if (password().isEmpty())
    {
        ui->messageLabel->setText(QStringLiteral("密码不能为空")); // 显示错误提示
        ui->passwordEdit->setFocus(); // 将焦点返回密码输入框
        return; // 停止本次登录
    }

    accept(); // 输入完整后关闭对话框并返回 Accepted
}

// 处理注册按钮点击
// 功能说明：校验注册输入的完整性，设置注册请求属性标记，并返回 Accepted 通知主窗口
// 参数说明：无
// 实现步骤：1. 检查注册用户名是否为空 2. 检查注册密码是否为空 3. 设置 registerRequested 属性 4. 关闭对话框并返回 Accepted
void LoginDialog::onRegisterClicked()
{
    // 检查注册用户名是否为空
    if (userId().isEmpty())
    {
        ui->messageLabel->setText(QStringLiteral("注册时用户名不能为空")); // 显示错误提示
        ui->usernameEdit->setFocus(); // 将焦点返回用户名输入框
        return; // 停止本次注册
    }

    // 检查注册密码是否为空
    if (password().isEmpty())
    {
        ui->messageLabel->setText(QStringLiteral("注册时密码不能为空")); // 显示错误提示
        ui->passwordEdit->setFocus(); // 将焦点返回密码输入框
        return; // 停止本次注册
    }

    // 使用对话框属性通知主窗口执行注册流程
    setProperty("registerRequested", true);

    // 关闭对话框并返回 Accepted
    accept();
}

// 处理取消按钮和关闭按钮点击
// 功能说明：放弃登录或注册操作，关闭对话框并返回 Rejected
// 参数说明：无
// 实现步骤：调用 reject 方法关闭对话框
void LoginDialog::onCancelClicked()
{
    reject(); // 关闭对话框并返回 Rejected
}
