#ifndef CODEJUDGE_PROBLEM_LIBRARY_H
#define CODEJUDGE_PROBLEM_LIBRARY_H

#include "common/types.h"
#include <string>
#include <vector>

// 题库管理类，用于题包导入、恢复、移除和题目查询。
// 本类通过题目列表和隐藏编号管理共享题库。
class ProblemLibrary
{
public:
    ProblemLibrary(std::string packagesDir); // 记录UTF-8题包保存目录

    bool load(std::string& error); // 恢复题库和隐藏标记，失败原因写入error
    bool importPackage(std::string folder, std::string& error); // 导入folder中的题包，或恢复同内容的隐藏题包
    std::vector<Problem> getProblems(); // 返回可见题目副本，未加载时返回空列表

    bool findProblem(
        std::string packageId,
        std::string problemId,
        Problem& result,
        std::string& error); // 按两个编号查题；失败时不修改result，原因写入error

    bool getTestCase(
        std::string packageId,
        std::string problemId,
        int testIndex,
        TestCase& result,
        std::string& error); // 读取题目的第testIndex个测试点；失败时不修改result

    std::vector<Problem> searchProblems(
        const ProblemQuery& query,
        const std::vector<Progress>& currentUserProgress); // 按query和当前用户进度筛选，返回题目副本
    bool removePackageFromLibrary(std::string packageId, std::string& error); // 隐藏指定题包，保留缓存和个人进度

private:
    bool isRemoved(const std::string& packageId) const; // 判断题包是否已隐藏
    bool saveRemoved(const std::vector<std::string>& candidate, std::string& error); // 保存隐藏编号，失败时保留旧文件

    std::vector<std::string> removedPackageIds_; // 已隐藏的题包编号
    std::string packagesDir_; // 题包副本的保存目录
    std::vector<Problem> problems_; // 全部已加载题目，包含隐藏题包的题目
    bool loaded_ = false; // 最近一次load是否成功，失败后禁止修改题库
};

#endif
