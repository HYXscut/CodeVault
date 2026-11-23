#include "mainwindow.h"
#include "logindialog.h"
#include <QApplication>
#include <QTimer>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QSignalSpy>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>
#include <cstdlib>

static void require(bool ok,const char* text)
{
    if (!ok) { std::cerr<<"FAIL "<<text<<'\n'; std::exit(1); }
    std::cout<<"PASS "<<text<<std::endl;
}
class LoginDriver : public QObject
{
    Q_OBJECT
public:
    QString username;
    QTimer timer;
    LoginDriver() { connect(&timer,&QTimer::timeout,this,&LoginDriver::fillLogin); }
public slots:
    void fillLogin()
    {
        LoginDialog* dialog=qobject_cast<LoginDialog*>(QApplication::activeModalWidget());
        if (!dialog) return;
        dialog->findChild<QLineEdit*>("usernameEdit")->setText(username);
        dialog->findChild<QLineEdit*>("passwordEdit")->setText("session-test-password");
        dialog->findChild<QPushButton*>("loginButton")->click();
    }
};
static void selectProblem(MainWindow& window)
{
    QTableWidget* table=window.findChild<QTableWidget*>("problemTable");
    for (int row=0;row<table->rowCount();++row)
    {
        if (table->item(row,0)->text()=="week3_demo" && table->item(row,1)->text()=="P001")
        {
            require(QMetaObject::invokeMethod(&window,"showSelectedProblems",Qt::DirectConnection,
                Q_ARG(int,row),Q_ARG(int,0)),"real problem-selection slot");
            return;
        }
    }
    require(false,"problem row exists");
}
int main(int argc,char** argv)
{
    QApplication app(argc,argv);
    const QString appDir=QCoreApplication::applicationDirPath();
    const QString data=appDir+"/data";
    require(!QFileInfo::exists(data),"fresh isolated session-test directory required");
    require(QDir().mkpath(data),"test data created");
    const std::string source=CODEVAULT_SOURCE_ROOT;
    std::string error,aliceId,bobId;
    AccountManager accounts((data+"/accounts.txt").toUtf8().toStdString());
    require(accounts.load(error) && accounts.registerAccount("alice","session-test-password",error) &&
        accounts.registerAccount("bob","session-test-password",error),"synthetic accounts");
    require(accounts.login("alice","session-test-password",aliceId,error),"Alice fixture identity"); accounts.logout();
    require(accounts.login("bob","session-test-password",bobId,error),"Bob fixture identity"); accounts.logout();
    ProblemLibrary library((data+"/packages").toUtf8().toStdString());
    require(library.load(error) && library.importPackage(source+"/samples/week3_demo",error),"real package fixture");
    ProgressStore store((data+"/progress.txt").toUtf8().toStdString());
    require(store.load(error),"progress fixture load");
    Progress a; a.userId=aliceId; a.packageId="week3_demo"; a.problemId="P001";
    a.sourcePath=source+"/examples/answer_output_limit.cpp"; a.status=2; a.lastSummary="ALICE_OLD";
    Progress b=a; b.userId=bobId; b.sourcePath=source+"/examples/answer_wa_at3.cpp"; b.status=1; b.lastSummary="BOB_WA_SAVED";
    require(store.saveProgress(a,error) && store.saveProgress(b,error),"distinct paths and summaries");
    LoginDriver login; login.username="alice"; login.timer.start(10);
    MainWindow window; login.timer.stop();
    require(window.isStartupReady() && window.windowTitle().contains("alice"),"MainWindow Alice login");
    require(window.findChild<QLabel*>("brandTitle")->text().contains("alice"),"visible Alice identity");
    selectProblem(window);
    require(window.findChild<QLineEdit*>("sourcePathEdit")->text().endsWith("answer_output_limit.cpp"),"Alice source restored");
    JudgeEngine* engine=window.findChild<JudgeEngine*>(); require(engine!=nullptr,"real window judge engine");
    QSignalSpy finished(engine,&JudgeEngine::judgeFinished);
    require(QMetaObject::invokeMethod(&window,"startJudge",Qt::DirectConnection),"real start-judge slot");
    require(finished.count()>0 || finished.wait(35000),"real Alice judgment completed");
    require(engine->getResult().status=="OutputLimit" && window.findChild<QLabel*>("resultLabel")->text().contains("alice"),"Alice OutputLimit result ownership label");
    login.username="bob"; login.timer.start(10);
    require(QMetaObject::invokeMethod(&window,"logoutAccount",Qt::DirectConnection),"real logout-and-Bob-login slot"); login.timer.stop();
    require(window.windowTitle().contains("bob") && window.findChild<QLabel*>("brandTitle")->text().contains("bob"),"visible Bob identity");
    require(window.findChild<QLineEdit*>("sourcePathEdit")->text().isEmpty(),"switch clears previous source");
    selectProblem(window);
    require(window.findChild<QLineEdit*>("sourcePathEdit")->text().endsWith("answer_wa_at3.cpp"),"Bob original source restored");
    require(window.findChild<QLabel*>("statusLabel")->text().contains("BOB_WA_SAVED"),"Bob original summary restored");
    require(store.load(error),"inspect persisted data");
    Progress actual;
    require(store.getProgress(bobId,"week3_demo","P001",actual,error) && actual.sourcePath==b.sourcePath &&
        actual.status==1 && actual.lastSummary==b.lastSummary,"Bob file record unchanged");
    require(store.getProgress(aliceId,"week3_demo","P001",actual,error) && actual.lastSummary.find("OutputLimit")==0,"Alice result saved to Alice");
    login.username="alice"; login.timer.start(10);
    require(QMetaObject::invokeMethod(&window,"logoutAccount",Qt::DirectConnection),"switch back to Alice"); login.timer.stop();
    selectProblem(window);
    require(window.findChild<QLineEdit*>("sourcePathEdit")->text().endsWith("answer_output_limit.cpp"),"Alice path retained separately");
    // 仅清理本测试刚创建的 data；不访问候选目录的个人数据。
    require(QFileInfo(data).absolutePath()==appDir && QDir(data).removeRecursively(),"isolated fixture cleanup");
}
#include "session_test.moc"
