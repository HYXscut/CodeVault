#ifndef CODEJUDGE_COMPILER_CHECKER_H
#define CODEJUDGE_COMPILER_CHECKER_H

#include "common/types.h"
#include <QObject>

class CompilerChecker : public QObject
{
    Q_OBJECT

  public:
    // 构造函数：初始化待检查的编译器路径（不主动发起检查）
    CompilerChecker(std::string compilerPath, QObject *parent = nullptr);

    // 异步发起编译器检查：非阻塞执行，若上一次检查未结束则返回 false
    bool checkCompiler();

    // 获取最近一次完成的检查结果
    CompilerInfo getCompilerInfo();

  signals:
    // 检查完成信号：当次检查结束（成功或失败）后发出
    void checkFinished();

    // 私有成员待补充。
};

#endif