#ifndef CODEJUDGE_TYPES_H
#define CODEJUDGE_TYPES_H
#include<vector>
#include<string>
struct Problem {
	std::string packageId;//题包编号
	std::string problemId;//题包内题目编号
	std::string title;//题目标题
	std::vector<std::string> tags;//考点标签
	std::string statement;//完整题目内容
	int timeLimitMs = 1000;//测试点时限（单位毫秒）
	int testCount = 0;//测试点个数
	std::string problemDir;//应用中保存的题目副本绝对目录
};
struct CompilerInfo
{
    bool found = false;  // 是否成功取得编译器版本
    std::string version; // 成功时的实际版本文字
    std::string error;   // 失败时的原因
};
#endif
