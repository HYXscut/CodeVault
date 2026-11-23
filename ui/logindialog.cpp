#include "logindialog.h" // 引入登录对话框类的声明
#include "ui_logindialog.h" // 引入 Qt Designer 生成的界面类

#include <QPushButton> // 使用按钮控件类型

// 创建登录对话框
LoginDialog::LoginDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoginDialog) // 创建界面对象
{
    ui->setupUi(this); // 根据 .ui 文件创建界面控件

    // 设置无边框窗口和窗口背景属性
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);

    // 设置登录窗口的整体样式
    setStyleSheet(R"(
        QDialog {
            background: white;
            border: 1px solid #D9DEE8;
            border-radius: 14px;
        }
        QFrame#leftPanel {
            border-image: url(:/login_background.svg) 0 0 0 0 stretch stretch;
            border-top-left-radius: 14px;
            border-bottom-left-radius: 14px;
        }
        QLabel#acBadge, QLabel#cppBadge {
            color: white;
            background: rgba(255, 255, 255, 35);
            border: 1px solid rgba(255, 255, 255, 90);
            border-radius: 7px;
            padding: 5px 8px;
            font-size: 12px;
        }
        QLabel#logoLabel {
            color: transparent;
            background: transparent;
            border: none;
            border-radius: 32px;
            min-width: 58px;
            max-width: 58px;
            min-height: 58px;
            max-height: 58px;
            font-size: 22px;
            font-weight: bold;
        }
        QLabel#brandLabel {
            color: white;
            font-size: 25px;
            font-weight: bold;
            margin-top: 8px;
        }
        QLabel#brandSubtitleLabel {
            color: #D8E2FF;
            font-size: 13px;
        }
        QLabel#versionLabel {
            color: #D8E2FF;
            font-size: 11px;
        }
        QFrame#rightPanel {
            background: white;
            border-top-right-radius: 14px;
            border-bottom-right-radius: 14px;
        }
        QLabel#titleLabel {
            color: #20242B;
            font-size: 22px;
            font-weight: bold;
        }
        QLabel#descriptionLabel {
            color: #8A919D;
            font-size: 12px;
        }
        QLabel#usernameLabel, QLabel#passwordLabel {
            color: #303641;
            font-size: 12px;
        }
        QLineEdit {
            min-height: 34px;
            border: 1px solid #D4DAE4;
            border-radius: 6px;
            padding: 0 10px;
            color: #252A33;
            background: #FFFFFF;
        }
        QLineEdit:focus {
            border: 2px solid #4D82E8;
            padding: 0 9px;
        }
        QPushButton#closeButton {
            color: #7E8794;
            border: none;
            background: transparent;
            font-size: 22px;
            min-width: 24px;
            max-width: 24px;
            min-height: 24px;
            max-height: 24px;
            padding: 0;
        }
        QPushButton#closeButton:hover {
            color: #252A33;
            background: #F0F2F5;
            border-radius: 12px;
        }
        QPushButton#loginButton, QPushButton#registerButton, QPushButton#cancelButton {
            min-height: 34px;
            min-width: 70px;
            border-radius: 6px;
            padding: 0 12px;
            font-size: 13px;
        }
        QPushButton#loginButton {
            color: white;
            background: #3269D7;
            border: 1px solid #3269D7;
        }
        QPushButton#loginButton:hover { background: #285BC3; }
        QPushButton#registerButton, QPushButton#cancelButton {
            color: #303641;
            background: white;
            border: 1px solid #D4DAE4;
        }
        QPushButton#registerButton:hover, QPushButton#cancelButton:hover {
            background: #F5F7FA;
        }
        QLabel#messageLabel {
            color: #D64545;
            font-size: 12px;
            min-height: 20px;
        }
    )");

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

LoginDialog::~LoginDialog()
{
    delete ui; // 释放界面对象
}

// 返回用户输入的用户名
QString LoginDialog::userId() const
{
    return ui->usernameEdit->text().trimmed(); // 去除用户名首尾空格
}

// 返回用户输入的密码
QString LoginDialog::password() const
{
    return ui->passwordEdit->text(); // 读取密码内容
}

// 处理登录按钮点击
void LoginDialog::onLoginClicked()
{
    // 检查用户名是否为空
    if (userId().isEmpty())
    {
        ui->messageLabel->setText(QStringLiteral("用户名不能为空")); // 显示错误提示
        ui->usernameEdit->setFocus(); // 将焦点返回用户名输入框
        return; // 停止本次登录
    }

    if (password().isEmpty())
    {
        ui->messageLabel->setText(QStringLiteral("密码不能为空")); // 显示错误提示
        ui->passwordEdit->setFocus(); // 将焦点返回密码输入框
        return; // 停止本次登录
    }

    accept(); // 输入完整后关闭对话框并返回 Accepted
}

// 处理注册按钮点击
void LoginDialog::onRegisterClicked()
{
    // 检查注册用户名是否为空
    if (userId().isEmpty())
    {
        ui->messageLabel->setText(QStringLiteral("注册时用户名不能为空")); // 显示错误提示
        ui->usernameEdit->setFocus(); // 将焦点返回用户名输入框
        return; // 停止本次注册
    }

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
void LoginDialog::onCancelClicked()
{
    reject(); // 关闭对话框并返回 Rejected
}