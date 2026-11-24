#include "progress_store.h"
#include <istream>
#include <sstream>
#include <string>
#include <vector>
#include <QFile>
#include <QFileInfo>
#include <QByteArray>
#include <QString>
#include <QSaveFile>

using namespace std;
// 以下函数用于读取一条进度记录
// input为输入流；result返回记录，error返回错误原因。
// 逻辑：读取固定八行，检查编号和状态，全部通过后赋值。
// 成功返回 true；失败返回 false，不修改 result。
static bool readOneProgress(
    istream& input,
    Progress& result,
    string& error)
{
    string fields[8]; // 每条记录固定八行，允许部分字段为空

    for (int i = 0; i < 8; ++i)
    {
        if (!getline(input, fields[i])) // 空行可以读到，缺行才失败
        {
            error = "Incomplete progress record";
            return false;
        }

        if (!fields[i].empty() &&
            fields[i][fields[i].size() - 1] == '\r')
        {
            fields[i].erase(fields[i].size() - 1);
        }
    }

    if (fields[0].empty() ||
        fields[1].empty() ||
        fields[2].empty())
    {
        error = "Progress IDs must not be empty";
        return false;
    }

    int status;
    if (fields[4] == "0")
        status = ProgressNotStarted;
    else if (fields[4] == "1")
        status = ProgressInProgress;
    else if (fields[4] == "2")
        status = ProgressLocalAccepted;
    else
    {
        error = "Invalid progress status";
        return false;
    }

    Progress complete;
    complete.userId = fields[0];
    complete.packageId = fields[1];
    complete.problemId = fields[2];
    complete.sourcePath = fields[3];
    complete.status = status;
    complete.lastSummary = fields[5];
    complete.lastOpenedTime = fields[6];
    complete.lastJudgedTime = fields[7];

    result = complete;
    error.clear();
    return true;
}
// 以下函数用于读取整份进度内容
// input为输入流；result返回记录列表，error返回错误原因。
// 逻辑：检查文件头和记录数，逐条读取并检查重复编号，最后确认没有多余内容。
// 成功返回 true；失败返回 false，不修改 result。
static bool readProgressData(
    istream& input,
    vector<Progress>& result,
    string& error)
{
    string header[2];

    for (int i = 0; i < 2; ++i)
    {
        if (!getline(input, header[i]))
        {
            error = "Missing progress header";
            return false;
        }

        if (!header[i].empty() &&
            header[i][header[i].size() - 1] == '\r')
        {
            header[i].erase(header[i].size() - 1);
        }
    }

    if (header[0] != "CJPROGRESS1")
    {
        error = "Invalid progress header";
        return false;
    }

    if (header[1].empty())
    {
        error = "Missing record count";
        return false;
    }

    for (int i = 0; i < header[1].size(); ++i)
    {
        if (header[1][i] < '0' || header[1][i] > '9')
        {
            error = "Invalid record count";
            return false;
        }
    }

    int count;
    istringstream countStream(header[1]);
    if (!(countStream >> count)) // 记录数超出 int 范围时也拒绝
    {
        error = "Record count is too large";
        return false;
    }

    vector<Progress> candidate;

    for (int i = 0; i < count; ++i)
    {
        Progress current;
        if (!readOneProgress(input, current, error))
            return false;

        for (int j = 0; j < candidate.size(); ++j)
        {
            if (candidate[j].userId == current.userId &&
                candidate[j].packageId == current.packageId &&
                candidate[j].problemId == current.problemId)
            {
                error = "Duplicate progress IDs";
                return false;
            }
        }

        candidate.push_back(current);
    }

    string extraLine;
    if (getline(input, extraLine)) // 规定数量之后不能再有额外行
    {
        error = "Extra progress data";
        return false;
    }

    if (input.bad() || !input.eof()) // 确认是正常读到文件末尾
    {
        error = "Progress stream read failed";
        return false;
    }

    result = candidate;
    error.clear();
    return true;
}
// 以下构造函数用于记录进度文件路径
// filePath为UTF-8绝对路径；这里只保存路径。
ProgressStore::ProgressStore(std::string filePath)
{
    filePath_ = filePath;
}
// 以下函数用于查询某个用户在指定题目上的进度
// userId、packageId、problemId确定记录；result返回进度，error返回错误原因。
// 逻辑：先检查是否加载成功，再遍历查找；找不到时返回未开始记录。
// 成功返回 true；失败返回 false，失败时不会修改 result。
bool ProgressStore::getProgress(
    std::string userId,
    std::string packageId,
    std::string problemId,
    Progress& result,
    std::string& error)
{
    if (!loaded_)
    {
        error = "Progress data is not loaded";
        return false;
    }

    for (int i = 0; i < records_.size(); ++i)
    {
        if (records_[i].userId == userId &&
            records_[i].packageId == packageId &&
            records_[i].problemId == problemId) // 确保三个编号都相同
        {
            result = records_[i];
            error.clear();
            return true;
        }
    }

    Progress defaultProgress; // 找不到记录，按未开始处理
    defaultProgress.userId = userId;
    defaultProgress.packageId = packageId;
    defaultProgress.problemId = problemId;

    result = defaultProgress;
    error.clear();
    return true;
}

// 以下函数用于读取进度文件
// error返回错误原因。
// 逻辑：检查路径，读取并解析文件，全部成功后更新记录；首次没有文件按空数据处理。
// 成功返回 true；失败返回 false，保留旧记录和文件，并禁止查询和保存。
bool ProgressStore::load(std::string& error)
{
    loaded_ = false; // 加载失败后禁止继续查询和保存

    if (filePath_.empty())
    {
        error = "Progress file path is empty";
        return false;
    }

    QString path = QString::fromUtf8(filePath_.c_str());

    QFileInfo info(path);
    if (!info.isAbsolute())
    {
        error = "Progress file path must be absolute";
        return false;
    }

    if (!info.exists())
    {
        if (info.isSymLink()) // 失效链接不能当成首次没有文件
        {
            error = "Progress file link is invalid";
            return false;
        }

        records_.clear();
        loaded_ = true;
        error.clear();
        return true;
    }

    if (!info.isFile())
    {
        error = "Progress path is not a regular file";
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        error = "Cannot open progress file: " + filePath_ +
                "; " + file.errorString().toUtf8().toStdString();
        return false;
    }

    QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError)
    {
        error = "Cannot read progress file: " + filePath_ +
                "; " + file.errorString().toUtf8().toStdString();
        return false;
    }

    istringstream input(bytes.toStdString());
    vector<Progress> candidate;

    if (!readProgressData(input, candidate, error))
    {
        return false;
    }

    records_ = candidate; // 整份读取或保存成功后才更新内存
    loaded_ = true;
    error.clear();
    return true;
}

// 以下函数用于取得某个用户的全部进度
// userId为用户编号；返回该用户的记录副本。
// 逻辑：先检查是否加载成功，再遍历筛选；未加载或没有记录时返回空列表。
std::vector<Progress> ProgressStore::getUserProgress(
    std::string userId)
{
    std::vector<Progress> result;

    if (!loaded_)
    {
        return result;
    }

    for (int i = 0; i < records_.size(); ++i)
    {
        if (records_[i].userId == userId)
        {
            result.push_back(records_[i]);
        }
    }

    return result;
}

// 以下函数用于保存完整的进度文本
// filePath为目标路径，text为文件内容，error返回错误原因。
// 逻辑：写入临时文件，确认全部写完后再替换原文件。
// 成功返回 true；失败返回 false，保留原文件。
static bool writeProgressText(
    std::string filePath,
    std::string text,
    std::string& error)
{
    QSaveFile file(QString::fromUtf8(filePath.c_str()));
    file.setDirectWriteFallback(false); // 禁止直接覆盖旧文件

    if (!file.open(QIODevice::WriteOnly))
    {
        error = "Cannot open progress save file: " +
                file.errorString().toUtf8().toStdString();
        return false;
    }

    QByteArray bytes = QByteArray::fromStdString(text);
    if (file.write(bytes) != bytes.size()) // 必须写完所有字节
    {
        error = "Cannot write progress data: " +
                file.errorString().toUtf8().toStdString();
        file.cancelWriting();
        return false;
    }

    if (!file.commit()) // 提交成功后才替换目标文件
    {
        error = "Cannot commit progress file: " +
                file.errorString().toUtf8().toStdString();
        return false;
    }

    error.clear();
    return true;
}

// 以下函数用于新增或更新一条进度
// progress为要保存的记录，error返回错误原因。
// 逻辑：检查字段，在副本中按三个编号替换或追加记录，再按固定格式保存。
// 成功返回 true并更新内存；失败返回 false，保留旧文件和旧记录。
bool ProgressStore::saveProgress(
    Progress progress,
    std::string& error)
{
    if (!loaded_)
    {
        error = "Progress data is not loaded";
        return false;
    }

    if (progress.userId.empty() ||
        progress.packageId.empty() ||
        progress.problemId.empty())
    {
        error = "Progress IDs must not be empty";
        return false;
    }

    if (progress.status != ProgressNotStarted &&
        progress.status != ProgressInProgress &&
        progress.status != ProgressLocalAccepted)
    {
        error = "Invalid progress status";
        return false;
    }

    std::string fields[7] = {
        progress.userId,
        progress.packageId,
        progress.problemId,
        progress.sourcePath,
        progress.lastSummary,
        progress.lastOpenedTime,
        progress.lastJudgedTime
    };

    for (int i = 0; i < 7; ++i)
    {
        if (fields[i].find('\n') != std::string::npos ||
            fields[i].find('\r') != std::string::npos)
        {
            error = "Progress fields must not contain line breaks";
            return false;
        }
    }

    std::vector<Progress> candidate = records_; // 在副本上修改，保存失败不影响旧记录
    bool found = false;

    for (int i = 0; i < candidate.size(); ++i)
    {
        if (candidate[i].userId == progress.userId &&
            candidate[i].packageId == progress.packageId &&
            candidate[i].problemId == progress.problemId)
        {
            candidate[i] = progress;
            found = true;
            break;
        }
    }

    if (!found)
    {
        candidate.push_back(progress);
    }

    std::ostringstream output;
    output << "CJPROGRESS1\n";
    output << candidate.size() << '\n';

    for (int i = 0; i < candidate.size(); ++i)
    {
        output << candidate[i].userId << '\n';
        output << candidate[i].packageId << '\n';
        output << candidate[i].problemId << '\n';
        output << candidate[i].sourcePath << '\n';
        output << candidate[i].status << '\n';
        output << candidate[i].lastSummary << '\n';
        output << candidate[i].lastOpenedTime << '\n';
        output << candidate[i].lastJudgedTime << '\n';
    }

    if (!writeProgressText(filePath_, output.str(), error))
    {
        return false;
    }

    records_ = candidate; // 整份读取或保存成功后才更新内存
    error.clear();
    return true;
}
