#ifndef CODEJUDGE_TYPES_H
#define CODEJUDGE_TYPES_H
#include<vector>
#include<string>
// 题目信息，只保存数据。
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
// 一个测试点的输入和标准输出，只保存数据。
struct TestCase {
    int index = 0; // 测试点编号从1开始，0表示尚未填入数据
    std::string input; // 对应编号.in文件的完整内容，保留空格和换行
    std::string expectedOutput; // 对应编号.out文件的完整内容，保留空格和换行
};
struct TestPointResult {
	int index = 0;
	std::string status;
	long long elapsedMs = 0;
	std::string error;
	std::string runtimeError;
	bool executed = false;
};
struct JudgeResult{
	//B判题后填写，C界面展示的数据
	std::string packageId;//本次判题的题包
	std::string problemId;//本次判题的题目编号
	std::string sourcePath;//本次使用的.cpp路径
	std::string status;//AC、WA、CE、TLE、RE、InternalError;
	int testIndex=0;//产生结果的测试点编号
	long long elapsedMs=0;//运行耗时，单位毫秒
	std::string actualOutput;//实际输出
	std::string expectedOutput;//标准输出
	std::string compilerOutput;//编译器给出的文字说明
	std::string runtimeError;//运行错误、超时说明或者程序错误的输出
	std::string error;//判题器自身参数、文件等错误
	int passedCount = 0;
	int executedCount = 0;
	int totalCount = 0;
	std::vector<TestPointResult> pointResults;
};
const int ProgressNotStarted = 0; // 未开始。
const int ProgressInProgress = 1; // 练习中。
const int ProgressLocalAccepted = 2; // 本地通过。

// 一个用户对一道题的最近练习记录，只保存数据，没有继承关系。
struct Progress
{
    std::string userId; // 用户编号
    std::string packageId; // 题包编号
    std::string problemId; // 题目编号
    std::string sourcePath; // 原始源码路径，未绑定时为空
    int status = ProgressNotStarted; // 默认未开始
    std::string lastSummary; // 最近判题的单行摘要
    std::string lastOpenedTime; // 最近打开时间，尚未记录时为空
    std::string lastJudgedTime; // 最近判题时间，尚未记录时为空
};

// 题目筛选条件，只保存数据，没有继承关系。
struct ProblemQuery
{
    std::string titleKeyword; // 标题包含的文字，区分大小写，空串表示不限
    std::string tag; // 精确匹配的标签，空串表示不限
    std::string packageId; // 题包编号，空串表示不限
    int status = -1; // 进度状态，-1表示不限
};
#endif
