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
struct TestCase{
	//作为测试点的数据载体
	int index=0;//测试点编号，0表示尚未填入数据，1表示本周成功读取
	std::string input;//从1.in读取的完整内容，保留空格和末尾换行
	std::string expectedOutput;//从1.out读取的完整标准输出，保留空格和换行
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
};
#endif
