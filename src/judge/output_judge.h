#ifndef OUTPUT_JUDGE_H
#define OUTPUT_JUDGE_H

#include <string>

// 输出比较器抽象基类（对应第 10 章抽象类与纯虚函数）
class OutputJudge
{
  public:
    // 虚析构函数：保证通过基类指针 delete 派生类对象时能正确释放资源
    virtual ~OutputJudge() {}

    // 纯虚函数：比较程序实际输出与标准输出是否一致
    virtual bool compare(const std::string &actual, const std::string &expected) const = 0;
};

// 文本输出比较器中间基类：提供统一换行符的公共辅助函数
class TextOutputJudge : public OutputJudge
{
  protected:
    // 将 Windows 风格换行 "\r\n" 统一转换为 "\n"
    static std::string normalizeNewlines(const std::string &text);
};

// 精确比较器派生类：统一换行符后逐字符完全匹配
class ExactJudge : public TextOutputJudge
{
  public:
    // 重写基类虚函数，执行精确文本比较
    virtual bool compare(const std::string &actual, const std::string &expected) const;
};

#endif