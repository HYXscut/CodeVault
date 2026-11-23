#include "account/account_manager.h"
#include "progress/progress_store.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <iostream>
#include <cstdlib>
#include <windows.h>

static int count=0;
static void check(bool ok,const char* name)
{
    if (!ok) { std::cerr<<"FAIL "<<name<<'\n'; std::exit(1); }
    ++count; std::cout<<"PASS "<<name<<'\n';
}
static void put(const QString& path,const QByteArray& text)
{
    QFile file(path); if (!file.open(QIODevice::WriteOnly) || file.write(text)!=text.size()) std::exit(2);
}
static QByteArray get(const QString& path)
{
    QFile file(path); if (!file.open(QIODevice::ReadOnly)) std::exit(3); return file.readAll();
}
static HANDLE lockFile(const QString& path)
{
    return CreateFileW((LPCWSTR)path.utf16(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);
}
int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);
    QTemporaryDir temp; check(temp.isValid(),"isolated data");
    const QString path=temp.path()+QString::fromUtf8("/中文 账号/accounts.txt");
    AccountManager manager(path.toUtf8().toStdString());
    std::string error,id="sentinel";
    check(!manager.login("alice","test-secret",id,error) && id=="sentinel","unloaded login blocked");
    check(manager.load(error) && get(path)=="CJACCOUNTS1\n0\n","first file and Chinese space path");
    const char* invalid[]={"","ab","abcdefghijklmnopqrstu","has space","has-dash","alice\n", "中文名"};
    for (const char* name:invalid) check(!manager.registerAccount(name,"test-secret",error),"invalid username rejected");
    check(!manager.registerAccount("alice","",error),"empty password rejected");
    check(manager.registerAccount("alice","test-secret",error),"register Alice");
    check(manager.registerAccount("bob","test-secret",error),"register Bob same password");
    const QList<QByteArray> fields=get(path).split('\n');
    check(fields[2]=="u0001" && fields[8]=="u0002","new stable IDs");
    check(fields[6]!=fields[12] && fields[7]!=fields[13],"same password distinct salts and verifiers");
    check(!get(path).contains("test-secret"),"file has no plaintext password");
    check(!manager.registerAccount("alice","other",error),"duplicate username rejected");
    check(manager.registerAccount("Alice","test-secret",error),"username case preserved");
    check(manager.registerAccount("a_1","test-secret",error),"letters digits underscore accepted");
    check(!manager.login("alice","wrong",id,error) && !manager.isLoggedIn() && id=="sentinel","wrong password leaves no session or output");
    check(manager.login("alice","test-secret",id,error) && id=="u0001" && manager.currentUserId()==id,"login returns internal ID");
    std::string second="unchanged";
    check(!manager.login("bob","test-secret",second,error) && second=="unchanged" && manager.currentUserId()=="u0001","switch requires logout");
    manager.logout(); check(!manager.isLoggedIn(),"logout clears session");
    check(manager.login("bob","test-secret",id,error) && id=="u0002","Bob independent identity");
    manager.logout();
    const QByteArray before=get(path);
    HANDLE handle=lockFile(path); check(handle!=INVALID_HANDLE_VALUE,"exclusive account lock");
    check(!manager.registerAccount("charlie","test-secret",error),"registration save failure");
    check(!manager.login("charlie","test-secret",id,error),"failed registration absent in memory");
    CloseHandle(handle); check(get(path)==before,"failed save preserves bytes");
    check(manager.registerAccount("charlie","test-secret",error),"register after unlock");
    check(manager.load(error) && manager.login("alice","test-secret",id,error) && id=="u0001","restart retains identity");
    manager.logout();
    const QByteArray valid=get(path);
    put(path,"CJACCOUNTS1\n1\nu0001\n");
    check(!manager.load(error) && !manager.registerAccount("other","test-secret",error),"truncated load blocks modification");
    check(!manager.isLoggedIn() && !manager.login("alice","test-secret",id,error),"failed load clears session");
    put(path,valid); check(manager.load(error),"repair reload");
    QList<QByteArray> broken=valid.split('\n'); broken[5]="1";
    put(path,broken.join('\n')); check(!manager.load(error),"unsupported iterations rejected");
    broken=valid.split('\n'); broken[6]="zz";
    put(path,broken.join('\n')); check(!manager.load(error),"bad salt rejected");
    broken=valid.split('\n'); broken[8]=broken[2];
    put(path,broken.join('\n')); check(!manager.load(error),"duplicate ID rejected");
    put(path,valid+"extra\n"); check(!manager.load(error),"extra account data rejected");
    const QString legacy=temp.path()+"/legacy.txt";
    put(legacy,"alice old-test-password\nu0001 another-test-password\n");
    AccountManager old(legacy.toUtf8().toStdString());
    check(old.load(error) && get(legacy).startsWith("CJACCOUNTS1\n2\n") && !get(legacy).contains("old-test-password"),"legacy atomic conversion");
    check(old.login("alice","old-test-password",id,error) && id=="alice","legacy progress ID preserved");
    old.logout();
    check(old.registerAccount("newuser","test-secret",error) && old.login("newuser","test-secret",id,error) && id=="u0002","new ID avoids legacy collision");
    ProgressStore progress((temp.path()+"/progress.txt").toUtf8().toStdString());
    check(progress.load(error),"legacy progress load");
    Progress record; record.userId="alice"; record.packageId="demo"; record.problemId="P001"; record.status=2;
    check(progress.saveProgress(record,error),"old identity progress fixture");
    old.logout(); check(old.load(error) && old.login("alice","old-test-password",id,error),"legacy converted restart login");
    Progress restored; check(progress.getProgress(id,"demo","P001",restored,error) && restored.status==2,"old progress found after conversion");
    const QString lockedLegacy=temp.path()+"/locked-legacy.txt";
    put(lockedLegacy,"tester test-password\n");
    handle=CreateFileW((LPCWSTR)lockedLegacy.utf16(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    check(handle!=INVALID_HANDLE_VALUE,"legacy readable but not replaceable lock");
    AccountManager blocked(lockedLegacy.toUtf8().toStdString());
    check(!blocked.load(error) && !blocked.login("tester","test-password",id,error),"failed conversion not enabled");
    CloseHandle(handle); check(get(lockedLegacy)=="tester test-password\n","failed conversion preserves original");
    const QString malformed=temp.path()+"/malformed.txt";
    put(malformed,"WRONG\n"); AccountManager bad(malformed.toUtf8().toStdString());
    check(!bad.load(error) && get(malformed)=="WRONG\n","bad legacy not overwritten");
    const QString known=temp.path()+"/known.txt";
    put(known,"CJACCOUNTS1\n1\nu0042\nknown\nPBKDF2-SHA256\n600000\n000102030405060708090a0b0c0d0e0f\n382b1e572fba3ac547369e7a7e3085617f972bf48e413a8d900e080f4c66a3bd\n");
    AccountManager reference(known.toUtf8().toStdString());
    check(reference.load(error) && reference.login("known","known-test-password",id,error) && id=="u0042","Python hashlib independent PBKDF2 reference");
    std::cout<<"TOTAL "<<count<<" ACCOUNT CHECKS PASS\n";
}
