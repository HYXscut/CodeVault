#ifndef CODEJUDGE_PROGRESS_STORE_H
#define CODEJUDGE_PROGRESS_STORE_H

#include "common/types.h"
#include <string>
#include <vector>

class ProgressStore // 管理练习记录及其文件读写；本类暂不参与继承。
{
public:
    ProgressStore(std::string filePath);
    // 参数 filePath：进度文件的 UTF-8 绝对路径。
    // 构造时只记录路径，不读取文件。

    bool load(std::string& error);
    // 成功加载整份文件后才替换内存记录，并清空 error。
    // 首次无文件视为正常空数据；损坏或不可读返回 false。
    // 失败时保留旧内存和原文件，并填写 error。

    bool getProgress(
        std::string userId,
        std::string packageId,
        std::string problemId,
        Progress& result,
        std::string& error);
    // 前提：最近一次 load 成功。
    // 按三个 ID 查找；无记录时返回带三个 ID 的默认未开始记录。
    // 成功返回 true 并清空 error；失败返回 false，result 不变。

    bool saveProgress(Progress progress, std::string& error);
    // 前提：最近一次 load 成功。
    // 按三个 ID 新增或替换记录，先保存候选数据。
    // 落盘成功才更新正式内存；失败保留旧文件和旧内存。

    std::vector<Progress> getUserProgress(std::string userId);
    // 在最近一次 load 成功后使用。
    // 返回指定用户的记录副本；没有记录时返回空集合。

private:
    std::string filePath_; // 进度文件位置。
    std::vector<Progress> records_; // 当前正式的内存记录。
    bool loaded_ = false; // 最近一次加载是否成功。
};

#endif