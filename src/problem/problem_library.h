#ifndef CODEJUDGE_PROBLEM_LIBRARY_H
#define CODEJUDGE_PROBLEM_LIBRARY_H

#include "common/types.h"
#include <string>
#include <vector>

class ProblemLibrary
{
  public:
    ProblemLibrary(std::string packagesDir);
    // 创建题库对象，packagesDir是应用保存题包副本的绝对目录

    bool load(std::string &error);
    // 加载应用目录中已保存的题包
    // error输出参数，返回错误原因
    // 首次运行没有题包时，准备好保存目录后返回true，此时为空题库
    // 失败返回false,不改变题库内容

    bool importPackage(std::string folder, std::string &error);
    // 导入外部题包，folder是用户选择的题包根目录
    //  若导入成功，error为空，保存题包副本;若失败，error返回导入失败原因，不改变题库内容

    std::vector<Problem> getProblems();
    // 返回当前题库中的全部题目，返回vector<Problem>副本，无题目时返回空列表

    bool findProblem(std::string packageId, std::string problemId, Problem &result, std::string &error);
    // 查找指定编号的题目，若查找失败返回false且error返回失败原因，不改变result值和题库内容；
    // 查找成功则error为空，result返回所查找的题目
};
#endif