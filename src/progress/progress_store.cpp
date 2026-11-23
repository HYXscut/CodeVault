#include "progress_store.h"
#include <istream>
#include <sstream>
#include <string>
#include <vector>

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