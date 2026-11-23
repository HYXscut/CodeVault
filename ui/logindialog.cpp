#include "logindialog.h"
#include "ui_logindialog.h"

LoginDialog::LoginDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoginDialog)
{
    ui->setupUi(this);

    // 设置密码输入显示方式,用圆点隐藏密码内容
    ui->passwordEdit->setEchoMode(QLineEdit::Password);

    // 连接登录处理函数
    connect(ui->loginButton,&QPushButton::clicked,this,&LoginDialog::onLoginClicked);
    // 连接注册处理函数
    connect(ui->registerButton,&QPushButton::clicked,this,&LoginDialog::onRegisterClicked);
    // 连接取消处理函数
    connect(ui->cancelButton,&QPushButton::clicked,this,&LoginDialog::onCancelClicked);
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

QString LoginDialog::userId() const // 获取用户名
{
    return ui->usernameEdit->text().trimmed(); // 去除用户名首尾空格
}

QString LoginDialog::password() const // 获取密码
{
    return ui->passwordEdit->text(); // 返回密码原文
}

void LoginDialog::onLoginClicked() // 处理登录按钮
{
    if (userId().isEmpty()) // 判断用户名是否为空
    {
        ui->messageLabel->setText(QStringLiteral("用户名不能为空"));

        return;
    }

    if (password().isEmpty()) // 判断密码是否为空
    {
        ui->messageLabel->setText(QStringLiteral("密码不能为空"));

        return;
    }

    accept(); // 输入合法后关闭对话框并返回 Accepted
}

void LoginDialog::onRegisterClicked() // 处理注册按钮
{
    if (userId().isEmpty())
    {
        ui->messageLabel->setText( QStringLiteral("注册时用户名不能为空"));

        return;
    }

    if (password().isEmpty())
    {
        ui->messageLabel->setText( QStringLiteral("注册时密码不能为空"));

        return;
    }
    // 保存当前操作类型、设置属性名称、标记用户点击了注册
    setProperty("registerRequested", true);

    accept(); // 关闭对话框并返回 Accepted
}

void LoginDialog::onCancelClicked() // 处理取消按钮
{
    reject(); // 关闭对话框并返回 Rejected
}