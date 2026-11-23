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
static bool readOneProgress(
    istream& input,
    Progress& result,
    string& error) // 从流中读取一条固定 8 行的记录。
{
    string fields[8]; // 暂存字段，全部检查成功后才填写 result。

    for (int i = 0; i < 8; ++i)
    {
        if (!getline(input, fields[i])) // 空行可以成功读到；缺行才失败。
        {
            error = "Incomplete progress record";
            return false;
        }

        if (!fields[i].empty() &&
            fields[i][fields[i].size() - 1] == '\r')
        {
            fields[i].erase(fields[i].size() - 1); // 只删除行末的回车。
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

    result = complete; // 完整读取并通过检查后才更新输出参数。
    error.clear();
    return true;
}
static bool readProgressData(
    istream& input,
    vector<Progress>& result,
    string& error) // 读取并校验整份进度内容，失败时不修改 result。
{
    string header[2]; // 第 1 行为标识，第 2 行为记录数。

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
    if (!(countStream >> count)) // 数字过大，无法存入 int 时也拒绝。
    {
        error = "Record count is too large";
        return false;
    }

    vector<Progress> candidate; // 暂存整份记录，不直接修改正式结果。

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

        candidate.push_back(current); // 通过检查后才加入候选集合。
    }

    string extraLine;
    if (getline(input, extraLine)) // 规定数量的记录之后不能再有额外行。
    {
        error = "Extra progress data";
        return false;
    }

    if (input.bad() || !input.eof()) // 确认是正常读到末尾，而非读取异常。
    {
        error = "Progress stream read failed";
        return false;
    }

    result = candidate; // 全部成功后，才整体替换调用方的结果。
    error.clear();
    return true;
}
ProgressStore::ProgressStore(std::string filePath)
{
    filePath_ = filePath; // 保存外部传入的进度文件路径。
}
bool ProgressStore::getProgress(
    std::string userId,
    std::string packageId,
    std::string problemId,
    Progress& result,
    std::string& error)
{
    if (!loaded_) // 尚未成功加载。
    {
        error = "Progress data is not loaded";
        return false; // 此时没有修改 result。
    }

    for (int i = 0; i < records_.size(); ++i) // 遍历对象内部的正式记录。
    {
        if (records_[i].userId == userId &&
            records_[i].packageId == packageId &&
            records_[i].problemId == problemId)
        {
            result = records_[i]; // 找到后，把记录副本交给调用方。
            error.clear();
            return true;
        }
    }

    Progress defaultProgress; // 没有已有记录，创建默认的未开始记录。
    defaultProgress.userId = userId;
    defaultProgress.packageId = packageId;
    defaultProgress.problemId = problemId;

    result = defaultProgress; // 默认状态和其余空字段也一起交给调用方。
    error.clear();
    return true;
}

bool ProgressStore::load(std::string& error)
{
    loaded_ = false; // 本次加载成功前，禁止查询和保存。

    if (filePath_.empty())
    {
        error = "Progress file path is empty";
        return false;
    }

    QString path = QString::fromUtf8(filePath_.c_str());
    // 将约定的 UTF-8 路径转换成 Qt 使用的字符串。

    QFileInfo info(path); // 查询路径对应的文件信息。
    if (!info.isAbsolute())
    {
        error = "Progress file path must be absolute";
        return false;
    }

    if (!info.exists())
    {
        if (info.isSymLink()) // 失效链接不能当成正常首次无文件。
        {
            error = "Progress file link is invalid";
            return false;
        }

        records_.clear(); // 首次无文件是正常空数据。
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
    if (!file.open(QIODevice::ReadOnly)) // 只读打开，不创建或清空文件。
    {
        error = "Cannot open progress file: " + filePath_ +
                "; " + file.errorString().toUtf8().toStdString();
        return false;
    }

    QByteArray bytes = file.readAll(); // 读取原始字节，保留换行。
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
        return false; // 解析失败，旧 records_ 保留。
    }

    records_ = candidate; // 整份解析成功后才替换正式内存。
    loaded_ = true;
    error.clear();
    return true;
}

std::vector<Progress> ProgressStore::getUserProgress(
    std::string userId)
{
    std::vector<Progress> result; // 暂存属于该用户的记录副本。

    if (!loaded_) // 防御性保护；调用方仍须先确认 load 成功。
    {
        return result;
    }

    for (int i = 0; i < records_.size(); ++i)
    {
        if (records_[i].userId == userId)
        {
            result.push_back(records_[i]); // 复制匹配的记录。
        }
    }

    return result;
}

static bool writeProgressText(
    std::string filePath,
    std::string text,
    std::string& error) // 完整提交文本，失败时保留原文件。
{
    QSaveFile file(QString::fromUtf8(filePath.c_str()));
    file.setDirectWriteFallback(false); // 无法使用临时文件时，直接报告失败。

    if (!file.open(QIODevice::WriteOnly))
    {
        error = "Cannot open progress save file: " +
                file.errorString().toUtf8().toStdString();
        return false;
    }

    QByteArray bytes = QByteArray::fromStdString(text);
    if (file.write(bytes) != bytes.size()) // 必须写入全部字节。
    {
        error = "Cannot write progress data: " +
                file.errorString().toUtf8().toStdString();
        file.cancelWriting(); // 放弃本次临时文件。
        return false;
    }

    if (!file.commit()) // 成功后才用临时文件替换目标文件。
    {
        error = "Cannot commit progress file: " +
                file.errorString().toUtf8().toStdString();
        return false;
    }

    error.clear();
    return true;
}

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

    std::vector<Progress> candidate = records_; // 复制正式集合。
    bool found = false;

    for (int i = 0; i < candidate.size(); ++i)
    {
        if (candidate[i].userId == progress.userId &&
            candidate[i].packageId == progress.packageId &&
            candidate[i].problemId == progress.problemId)
        {
            candidate[i] = progress; // 找到同一记录，替换候选副本。
            found = true;
            break;
        }
    }

    if (!found)
    {
        candidate.push_back(progress); // 没有已有记录，追加候选记录。
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
        return false; // 保存失败，正式 records_ 从未被修改。
    }

    records_ = candidate; // 落盘成功后，才更新正式内存。
    error.clear();
    return true;
}