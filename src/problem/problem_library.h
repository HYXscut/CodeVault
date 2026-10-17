#ifndef CODEJUDGE_PROBLEM_LIBRARY_H
#define CODEJUDGE_PROBLEM_LIBRARY_H

#include "common/types.h"
#include <string>
#include <vector>

class ProblemLibrary
// 题库管理类：负责从应用保存目录加载题包、导入外部题包，以及查询题目。
// 本类不继承其他类，也不作为其他类的基类；它通过 problems_ 集合管理 Problem 对象。
// packagesDir_ 保存题包副本所在目录；problems_ 保存当前已加载的题目；
// loaded_ 表示 load() 是否曾成功完成，导入题包前必须为 true。
{
public:
    ProblemLibrary(std::string packagesDir);
    // 创建题库对象，记录应用保存题包副本的目录。
    // 参数 packagesDir：题包保存目录的绝对路径，按 UTF-8 字符串传入。
    // 实现：将参数移动到 packagesDir_；构造时不读取文件，也不创建目录。

    bool load(std::string& error);
    // 功能：读取保存目录下的正式题包，重建当前题库。
    // 参数 error：输出参数；成功时清空，失败时写入错误原因。
    // 返回值：全部题包解析成功返回 true；任一步失败返回 false。
    // 步骤：准备保存目录；收集并排序题包目录；逐个验证、解析；全部成功后一次性替换 problems_，并将 loaded_ 设为 true。
    // 首次运行且目录中没有题包时，返回 true，题库为空。
    // 解析失败时不替换已有题目；准备目录的过程仍可能创建目录。

    bool importPackage(std::string folder, std::string& error);
    // 功能：验证外部题包，将副本保存到应用目录，并加入当前题库。
    // 参数 folder：用户选择的外部题包根目录。
    // 参数 error：输出参数；成功时清空，失败时写入错误原因。
    // 返回值：导入成功返回 true，失败返回 false。
    // 步骤：检查加载状态和源题包；检查编号冲突；复制到临时目录；验证副本；重命名为正式目录；从最终位置解析；更新 problems_。
    // 普通失败路径不会更新内存中的题目；文件清理失败时可能残留副本目录。

    std::vector<Problem> getProblems();
    // 功能：取得当前题库中的全部题目。
    // 返回值：problems_ 的副本；没有题目时返回空 vector。
    // 实现：利用 vector 的复制操作，调用方修改返回值不会修改题库。

    bool findProblem(
        std::string packageId,
        std::string problemId,
        Problem& result,
        std::string& error);
    // 功能：按题包编号和题目编号查找题目。
    // 参数 packageId：目标题包编号。
    // 参数 problemId：目标题目编号。
    // 参数 result：输出参数；成功时写入题目副本，失败时保持原值。
    // 参数 error：输出参数；成功时清空，失败时写入未找到的原因。
    // 返回值：找到题目返回 true，否则返回 false。
    // 实现：顺序遍历 problems_，同时比较两个编号；只有匹配后才赋值 result。

private:
    std::string packagesDir_;
    // 应用保存题包副本的目录路径。

    std::vector<Problem> problems_;
    // 当前题库的题目集合；每个元素记录所属题包、题目内容及测试文件目录。

    bool loaded_ = false;
    // 是否已成功执行 load()；用于阻止在题库初始化前导入。
};

#endif