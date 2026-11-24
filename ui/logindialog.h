#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QString>

namespace Ui {
class LoginDialog;
}

/*
 类说明：登录对话框类，提供用户登录与注册界面的呈现、用户凭证输入及基础格式校验。
 继承与派生：公有继承自 QDialog
 类的属性说明：
    ui: Qt Designer 生成的界面控件管理对象指针（Ui::LoginDialog*）
 类的方法说明：
    LoginDialog(): 构造函数，初始化登录对话框界面与信号槽连接
    ~LoginDialog(): 析构函数，释放界面资源
    userId(): 获取用户在界面中输入的用户名
    password(): 获取用户在界面中输入的密码
    onLoginClicked(): 处理登录按钮点击，校验输入完整性并提交登录请求（私有槽函数）
    onRegisterClicked(): 处理注册按钮点击，校验输入完整性并设置注册请求标记（私有槽函数）
    onCancelClicked(): 处理取消及关闭按钮点击，退出对话框（私有槽函数）
 */
class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    // 创建登录对话框对象
    // 参数说明：parent - 父窗口指针，默认为 nullptr
    explicit LoginDialog(QWidget *parent = nullptr);

    // 释放界面对象并析构
    ~LoginDialog();

    // 获取用户输入的用户名
    // 参数说明：无
    // 返回值：去除首尾空格后的用户名字符串
    QString userId() const; // 获取用户名

    // 获取用户输入的密码
    // 参数说明：无
    // 返回值：用户输入的密码字符串
    QString password() const; // 获取密码

private slots:
    // 处理登录操作
    // 功能说明：响应登录按钮点击事件，验证输入并接受登录
    void onLoginClicked(); // 处理登录按钮

    // 处理注册操作
    // 功能说明：响应注册按钮点击事件，验证输入并标记注册请求
    void onRegisterClicked(); // 处理注册按钮

    // 处理取消与关闭操作
    // 功能说明：响应取消和右上角关闭按钮点击事件，放弃操作并退出
    void onCancelClicked(); // 处理取消按钮

private:
    // Qt Designer 生成的界面管理对象指针
    Ui::LoginDialog *ui;
};

#endif // LOGINDIALOG_H
