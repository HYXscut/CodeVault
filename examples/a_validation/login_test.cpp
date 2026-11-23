#include "logindialog.h"
#include <QApplication>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTest>
#include <QFontDatabase>
#include <QFont>
#include <iostream>
#include <cstdlib>

static void check(bool ok,const char* name)
{
    if (!ok) { std::cerr<<"FAIL "<<name<<'\n'; std::exit(1); }
    std::cout<<"PASS "<<name<<'\n';
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv);
    const int fontId=QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    if (fontId>=0) app.setFont(QFont(QFontDatabase::applicationFontFamilies(fontId).front(),9));
    LoginDialog dialog;
    dialog.show(); QCoreApplication::processEvents();
    QLineEdit* user=dialog.findChild<QLineEdit*>("usernameEdit");
    QLineEdit* password=dialog.findChild<QLineEdit*>("passwordEdit");
    QLineEdit* confirm=dialog.findChild<QLineEdit*>("confirmPasswordEdit");
    QPushButton* reg=dialog.findChild<QPushButton*>("registerButton");
    QPushButton* login=dialog.findChild<QPushButton*>("loginButton");
    QLabel* message=dialog.findChild<QLabel*>("messageLabel");
    check(user && password && confirm && reg && login && message,"dialog controls");
    check(!confirm->isVisible(),"login hides password confirmation");
    QTest::mouseClick(reg,Qt::LeftButton); QCoreApplication::processEvents();
    check(confirm->isVisible() && reg->text()==QStringLiteral("确认注册"),"registration mode");
    check(password->echoMode()==QLineEdit::Password && confirm->echoMode()==QLineEdit::Password,"both passwords masked");
    user->setText("alice"); password->setText("test-secret"); confirm->setText("different");
    QTest::mouseClick(reg,Qt::LeftButton);
    check(dialog.isVisible() && message->text()==QStringLiteral("两次密码不一致"),"mismatch keeps dialog open");
    if (argc==2) check(dialog.grab().save(QString::fromLocal8Bit(argv[1])),"registration preview saved");
    QTest::mouseClick(login,Qt::LeftButton);
    check(!confirm->isVisible() && !dialog.property("registerRequested").toBool(),"return to login");
    QTest::mouseClick(reg,Qt::LeftButton); confirm->setText("test-secret");
    QTest::mouseClick(reg,Qt::LeftButton);
    check(dialog.result()==QDialog::Accepted && dialog.property("registerRequested").toBool(),"matching passwords submit registration");
    LoginDialog second;
    second.findChild<QLineEdit*>("usernameEdit")->setText(" alice ");
    check(second.userId()==" alice ","username not silently trimmed");
}
