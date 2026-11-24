#ifndef CODEJUDGE_PROGRESS_STORE_H
#define CODEJUDGE_PROGRESS_STORE_H

#include "common/types.h"
#include <string>
#include <vector>

// 进度管理类，用于读取、查询和保存各用户的练习记录。
// 本类通过记录列表管理Progress对象。
class ProgressStore
{
public:
    ProgressStore(std::string filePath); // 记录UTF-8进度文件绝对路径，不读取文件
    bool load(std::string& error); // 读取全部记录，失败原因写入error

    bool getProgress(
        std::string userId,
        std::string packageId,
        std::string problemId,
        Progress& result,
        std::string& error); // 按三个编号查询；没有记录时返回未开始记录，失败时不修改result

    bool saveProgress(Progress progress, std::string& error); // 新增或更新记录，保存失败时保留旧文件和内存
    std::vector<Progress> getUserProgress(std::string userId); // 返回该用户的记录副本，未加载时返回空列表

private:
    std::string filePath_; // 进度文件路径
    std::vector<Progress> records_; // 已加载的进度记录
    bool loaded_ = false; // 最近一次load是否成功，失败后禁止查询和保存
};

#endif
