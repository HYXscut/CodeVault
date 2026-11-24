#include "problem/problem_library.h"
#include "progress/progress_store.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <iostream>
#include <cstdlib>
#include <windows.h>

static int checks = 0;
// 以下函数用于检查一项测试结果
// ok为是否通过，name为检查名称。
// 逻辑：失败时输出名称并退出程序，通过时累计次数并输出结果。
static void check(bool ok, const char* name)
{
    if (!ok) { std::cerr << "FAIL " << name << '\n'; std::exit(1); }
    ++checks;
    std::cout << "PASS " << name << '\n';
}
// 以下函数用于写入测试数据
// path为路径，bytes为内容。
// 逻辑：创建父目录，再写入全部内容；写入失败时结束测试。
static void put(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) std::exit(2);
}
// 以下函数用于读取测试文件
// path为路径，返回文件内容。
// 逻辑：只读打开后读取全部字节，打不开时结束测试。
static QByteArray get(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) std::exit(3);
    return file.readAll();
}
// 以下函数用于转换Qt字符串
// text为Qt字符串，返回UTF-8字符串，供题库和进度接口使用。
static std::string utf8(const QString& text) { return text.toUtf8().toStdString(); }
// 以下函数用于生成筛选用题包
// folder为目录，id为题包编号。
// 逻辑：生成25题和测试文件，用奇偶题号区分标题及标签。
static void fixture(const QString& folder, const std::string& id)
{
    std::string meta = "CJPKG1\n" + id + "\nFilter fixture\n25\n";
    for (int i = 1; i <= 25; ++i)
    {
        const std::string pid = "P" + std::string(i < 10 ? "00" : "0") + std::to_string(i);
        meta += pid + "\n" + (i % 2 == 0 ? "Loop " : "Array ") + std::to_string(i) +
            "\n" + (i % 2 == 0 ? "loop" : "array") + ";basic\n1000\n1\n";
        const QString dir = folder + "/" + QString::fromStdString(pid);
        put(dir + "/statement.md", "# Fixture\n");
        put(dir + "/1.in", "1 2\n"); put(dir + "/1.out", "3\n");
    }
    put(folder + "/package.txt", QByteArray::fromStdString(meta));
}
// 以下函数用于整理题目标识
// problems为题目列表，返回题包编号和题号拼接的文字。
// 逻辑：逐题拼接，用于核对筛选结果及顺序。
static std::string keys(const std::vector<Problem>& problems)
{
    std::string result;
    for (const Problem& p : problems) result += p.packageId + "/" + p.problemId + ";";
    return result;
}
// 以下程序用于检查A的题库、筛选和进度功能
// argc和argv接收命令行参数；--fixtures加新目录路径时只生成筛选题包。
// 逻辑：在临时目录中检查导入、筛选、隐藏恢复、两用户进度和保存失败保护。
// 全部通过返回0；任一检查失败时非零退出。
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (argc == 3 && std::string(argv[1]) == "--fixtures")
    {
        const QString target = QString::fromLocal8Bit(argv[2]);
        check(!QFileInfo::exists(target), "fixture destination must be new");
        fixture(target + "/filter_a", "filter_a"); fixture(target + "/filter_b", "filter_b");
        std::cout << "Generated 50 records.\n";
        return 0;
    }
    QTemporaryDir temp;
    check(temp.isValid(), "isolated temp directory");
    const QString root = temp.path();
    const QString a = root + QString::fromUtf8("/中文 题包/a");
    const QString b = root + "/sources/b";
    fixture(a, "filter_a"); fixture(b, "filter_b");
    QString data = root + "/data";
    ProblemLibrary library(utf8(data + "/packages"));
    std::string error;
    check(library.load(error) && library.getProblems().empty(), "first empty load");
    check(library.importPackage(utf8(a), error), "import Chinese space path");
    check(library.importPackage(utf8(b), error), "import second package");
    check(library.getProblems().size() == 50, "50 fixture records");
    ProgressStore store(utf8(data + "/progress.txt"));
    check(store.load(error), "first progress load");
    Progress pa; pa.userId="alice"; pa.packageId="filter_a"; pa.problemId="P001"; pa.status=2;
    pa.sourcePath="C:/source/alice.cpp";
    check(store.saveProgress(pa,error), "save Alice accepted");
    Progress pb=pa; pb.packageId="filter_b"; pb.status=1;
    check(store.saveProgress(pb,error), "save same problem ID other package");
    pb.packageId="filter_a"; pb.problemId="P002";
    check(store.saveProgress(pb,error), "save Alice practice");
    pb=pa; pb.userId="bob"; pb.status=1; pb.sourcePath="C:/source/bob.cpp";
    check(store.saveProgress(pb,error), "save Bob independent state");
    const std::vector<Progress> alice = store.getUserProgress("alice");
    const std::vector<Progress> bob = store.getUserProgress("bob");
    ProblemQuery q;
    check(library.searchProblems(q,alice).size()==50,"empty conditions");
    q.titleKeyword="Loop";
    check(library.searchProblems(q,alice).size()==24,"title substring direction");
    q.titleKeyword="loop";
    check(library.searchProblems(q,alice).empty(),"title case sensitive");
    q.titleKeyword="Loop 2 extra";
    check(library.searchProblems(q,alice).empty(),"keyword longer than title");
    q=ProblemQuery(); q.tag="loop";
    check(library.searchProblems(q,alice).size()==24,"exact tag");
    q.tag="loo"; check(library.searchProblems(q,alice).empty(),"tag prefix rejected");
    q=ProblemQuery(); q.packageId="filter_a";
    check(library.searchProblems(q,alice).size()==25,"package filter");
    q=ProblemQuery(); q.status=2;
    check(keys(library.searchProblems(q,alice))=="filter_a/P001;","Alice accepted exact key");
    check(library.searchProblems(q,bob).empty(),"Bob has no accepted");
    q.status=1;
    check(keys(library.searchProblems(q,alice))=="filter_a/P002;filter_b/P001;","practice exact keys across packages");
    check(keys(library.searchProblems(q,bob))=="filter_a/P001;","Bob practice exact key");
    q.status=0; check(library.searchProblems(q,alice).size()==47,"missing progress is not started");
    q=ProblemQuery(); q.titleKeyword="Loop"; q.tag="loop"; q.packageId="filter_a"; q.status=1;
    check(keys(library.searchProblems(q,alice))=="filter_a/P002;","four conditions AND");
    q.status=2; check(library.searchProblems(q,alice).empty(),"combined no match");
    const QByteArray progressBefore=get(data+"/progress.txt");
    check(library.removePackageFromLibrary("filter_a",error),"remove commits marker");
    check(library.getProblems().size()==25,"remove visibility");
    Problem unchanged; unchanged.title="sentinel";
    check(!library.findProblem("filter_a","P001",unchanged,error) && unchanged.title=="sentinel","hidden find preserves output");
    check(library.load(error) && library.getProblems().size()==25,"restart hidden");
    const QString marker=data+"/removed_packages.txt";
    const QByteArray markerBefore=get(marker);
    // 独占锁定测试文件，模拟无法保存；不操作正式运行数据
    HANDLE lock=CreateFileW((LPCWSTR)marker.utf16(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);
    check(lock!=INVALID_HANDLE_VALUE,"exclusive marker lock");
    check(!library.importPackage(utf8(a),error) && library.getProblems().size()==25,"restore save failure keeps hidden");
    CloseHandle(lock);
    check(get(marker)==markerBefore,"save failure preserves marker bytes");
    put(a+"/P001/statement.md","changed\n");
    check(!library.importPackage(utf8(a),error) && library.getProblems().size()==25,"changed statement rejected");
    put(a+"/P001/statement.md","# Fixture\n");
    put(a+"/P001/1.out","4\n");
    check(!library.importPackage(utf8(a),error),"changed answer rejected");
    put(a+"/P001/1.out","3\n");
    QByteArray meta=get(a+"/package.txt"); QByteArray changed=meta; changed.replace("1000","1001");
    put(a+"/package.txt",changed);
    check(!library.importPackage(utf8(a),error),"changed metadata rejected");
    put(a+"/package.txt",meta);
    check(QFile::remove(a+"/P001/1.in"),"remove fixture input");
    check(!library.importPackage(utf8(a),error),"missing input rejected");
    put(a+"/P001/1.in","1 2\n");
    check(library.importPackage(utf8(a),error) && library.getProblems().size()==50,"same bytes restore");
    check(get(data+"/progress.txt")==progressBefore,"remove restore preserves all progress bytes");
    check(!library.importPackage(utf8(a),error) && library.getProblems().size()==50,"visible duplicate rejected");
    check(!library.importPackage(utf8(root),error),"wrong folder rejected");
    changed=meta; changed.replace("P002\n","P001\n"); put(a+"/package.txt",changed);
    check(!library.importPackage(utf8(a),error),"duplicate problem ID rejected");
    changed=meta; changed.replace("1000","0"); put(a+"/package.txt",changed);
    check(!library.importPackage(utf8(a),error),"invalid time limit rejected");
    put(a+"/package.txt",meta); put(a+"/P001/2.out","0\n");
    check(!library.importPackage(utf8(a),error),"extra unpaired test rejected");
    QFile::remove(a+"/P001/2.out");
    lock=CreateFileW((LPCWSTR)marker.utf16(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);
    check(lock!=INVALID_HANDLE_VALUE,"second marker lock");
    check(!library.removePackageFromLibrary("filter_b",error) && library.getProblems().size()==50,"remove failure keeps visible");
    CloseHandle(lock);
    // 损坏隐藏标记后应禁止修改，修复后能重新加载
    put(marker,"../bad\n");
    check(!library.load(error),"corrupt marker rejected");
    check(!library.removePackageFromLibrary("filter_b",error),"mutation blocked after failed load");
    put(marker,"filter_b\n");
    check(library.load(error) && library.getProblems().size()==25,"repair marker reload");
    // 移动数据目录后重新加载，检查路径和两用户进度
    const QString moved=root+"/moved";
    check(QDir().rename(data,moved),"move entire data directory");
    ProblemLibrary relocated(utf8(moved+"/packages"));
    check(relocated.load(error) && relocated.getProblems().size()==25,"moved library and marker recover");
    check(QDir::fromNativeSeparators(QString::fromUtf8(relocated.getProblems()[0].problemDir.c_str())).startsWith(moved + "/"),"cached problem paths recomputed");
    ProgressStore recovered(utf8(moved+"/progress.txt"));
    check(recovered.load(error) && recovered.getUserProgress("alice").size()==3 && recovered.getUserProgress("bob").size()==1,"moved two-user progress recover");
    const QString progressFile = moved + "/progress.txt";
    lock=CreateFileW((LPCWSTR)progressFile.utf16(),GENERIC_READ,0,nullptr,OPEN_EXISTING,0,nullptr);
    check(lock!=INVALID_HANDLE_VALUE,"progress exclusive lock");
    Progress candidate=pa; candidate.status=1;
    check(!recovered.saveProgress(candidate,error),"progress save failure");
    Progress old;
    check(recovered.getProgress("alice","filter_a","P001",old,error) && old.status==2,"progress save failure preserves memory");
    CloseHandle(lock);
    check(get(progressFile)==progressBefore,"progress save failure preserves bytes");
    put(moved+"/progress.txt","WRONG\n");
    check(!recovered.load(error) && !recovered.saveProgress(pa,error),"corrupt progress blocks overwrite");
    put(moved+"/progress.txt",progressBefore);
    check(recovered.load(error),"progress repair reload");
    std::cout << "TOTAL " << checks << " PASS\n";
}
