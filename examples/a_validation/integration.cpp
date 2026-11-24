#include "problem/problem_library.h"
#include "progress/progress_store.h"
#include "account/account_manager.h"
#include "judge/judge_engine.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QDir>
#include <iostream>
#include <cstdlib>

// 以下函数用于检查联调结果
// ok为是否通过，message为失败说明。
// 逻辑：失败时输出说明并退出，通过时继续测试。
static void require(bool ok, const char* message)
{
    if (!ok) { std::cerr << "FAIL " << message << '\n'; std::exit(1); }
}
// 以下程序用于检查A的数据与账号、判题接口能否配合
// argc和argv接收g++.exe绝对路径，以及可选的样例根目录。
// 逻辑：导入真实题包，运行七次判题并核对结果，再检查两用户进度和题包恢复。
// 全部通过返回0；失败时输出原因并非零退出。
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    require(argc==2 || argc==3,"pass g++.exe absolute path and optional sample root");
    QTemporaryDir temp;
    require(temp.isValid(),"temporary data");
    const std::string root=argc==3 ? argv[2] : CODEVAULT_SOURCE_ROOT;
    const std::string data=temp.path().toUtf8().toStdString();
    std::string error;
    ProblemLibrary library(data+"/packages");
    require(library.load(error),"library load");
    require(library.importPackage(root+"/samples/demo",error),"formal three-problem package");
    require(library.importPackage(root+"/samples/week3_demo",error),"formal five-point package");
    require(library.getProblems().size()==4,"four formal demonstration problems");
    Problem problem;
    require(library.findProblem("week3_demo","P001",problem,error),"find five-point problem");
    std::vector<TestCase> tests;
    for (int i=1;i<=5;++i)
    {
        TestCase test;
        require(library.getTestCase("week3_demo","P001",i,test,error),"read real test files");
        tests.push_back(test);
    }
    ProgressStore store(data+"/progress.txt"); require(store.load(error),"progress load");
    AccountManager accounts(data+"/accounts.txt");
    require(accounts.load(error) && accounts.registerAccount("alice","integration-test",error) &&
            accounts.registerAccount("bob","integration-test",error),"register two real accounts");
    std::string aliceId,bobId;
    require(accounts.login("alice","integration-test",aliceId,error),"Alice login"); accounts.logout();
    require(accounts.login("bob","integration-test",bobId,error),"Bob login"); accounts.logout();
    require(aliceId!=bobId && aliceId!="practice" && bobId!="practice","independent stable IDs");
    JudgeEngine engine(argv[1]);
    // 依次检查AC、第三点WA、CE、RE、TLE、输出超限，最后再跑AC
    const char* sources[]={"answer_multi_ac.cpp","answer_wa_at3.cpp","answer_ce.cpp","answer_re.cpp",
        "answer_tle.cpp","answer_output_limit.cpp","answer_multi_ac.cpp"};
    const char* statuses[]={"AC","WA","CE","RE","TLE","OutputLimit","AC"};
    const int passed[]={5,2,0,0,0,0,5};
    const int executed[]={5,3,0,1,1,1,5};
    for (int i=0;i<7;++i)
    {
        QSignalSpy finished(&engine,&JudgeEngine::judgeFinished);
        const std::string source=root+"/examples/"+sources[i];
        require(engine.startJudge(problem,source,tests,error),error.c_str());
        if (finished.count()==0) require(finished.wait(35000),"judge completion timeout");
        QCoreApplication::processEvents();
        // 完成通知只能有一次，任务结束后引擎应恢复空闲
        require(finished.count()==1 && !engine.isBusy(),"one completion and engine idle");
        const JudgeResult result=engine.getResult();
        std::cout << sources[i] << ": " << result.status << ' ' << result.passedCount << '/'
                  << result.executedCount << '/' << result.totalCount << '\n';
        require(result.status==statuses[i] && result.passedCount==passed[i] &&
                result.executedCount==executed[i] && result.totalCount==5,"expected result and counts");
        require(result.pointResults.size()==5,"all point results present");
        for (int j=executed[i];j<5;++j)
            require(!result.pointResults[j].executed && result.pointResults[j].status=="NotRun","unexecuted points");
        // 只有第三点WA保存给bob，其他结果保存给alice
        Progress progress; progress.userId=i==1 ? bobId : aliceId;
        progress.packageId=problem.packageId; progress.problemId=problem.problemId; progress.sourcePath=source;
        progress.status=result.status=="AC" ? 2 : 1; progress.lastSummary=result.status;
        require(store.saveProgress(progress,error),"save real result");
    }
    require(accounts.load(error),"restart accounts");
    std::string restartedId;
    require(accounts.login("alice","integration-test",restartedId,error) && restartedId==aliceId,"restart stable identity");
    require(store.load(error),"restart progress load");
    Progress a,b;
    require(store.getProgress(aliceId,"week3_demo","P001",a,error) && a.status==2,"Alice final AC");
    require(store.getProgress(bobId,"week3_demo","P001",b,error) && b.status==1 && b.lastSummary=="WA","Bob WA retained");
    require(library.removePackageFromLibrary("week3_demo",error) && library.load(error),"remove and restart");
    require(library.importPackage(root+"/samples/week3_demo",error),"restore formal package");
    require(store.load(error) && store.getUserProgress(aliceId).size()==1 && store.getUserProgress(bobId).size()==1,"both users retained");
    require(QDir(QCoreApplication::applicationDirPath()+"/work").entryList(QDir::Dirs|QDir::NoDotAndDotDot).empty(),"work cleanup");
    std::cout << "PASS 7 real judge runs, counts, signals, cleanup and two-user recovery\n";
}
