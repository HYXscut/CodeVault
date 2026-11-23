#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QString>

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

    QString userId() const; // 获取用户名
    QString password() const; // 获取密码

private slots:
    void onLoginClicked(); // 处理登录按钮
    void onRegisterClicked(); // 处理注册按钮
    void onCancelClicked(); // 处理取消按钮

private:
    Ui::LoginDialog *ui;
};

#endif // LOGINDIALOG_H
