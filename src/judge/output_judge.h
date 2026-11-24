// 输出比较器模块：定义三层继承结构，用来比对程序实际输出和标准答案
#ifndef OUTPUT_JUDGE_H
#define OUTPUT_JUDGE_H

#include <string>

// 输出比较器的抽象基类，不继承其他类，作为所有具体比较方式的统一父类。
// 本类不含数据成员，只提供虚析构函数和纯虚函数 compare()；
// 判题引擎通过 OutputJudge* 基类指针调用 compare()，方便以后扩展不同的比较规则。
class OutputJudge
{
  public:
    // 虚析构函数：通过基类指针释放派生类对象时，能保证子类析构函数也被正确调用
    virtual ~OutputJudge() {}

    // 纯虚函数：判断实际输出 actual 和标准答案 expected 是否一致，一致返回 true，由子类具体实现
    virtual bool compare(const std::string &actual, const std::string &expected) const = 0;
};

// 文本比较器的中间基类，公有继承自 OutputJudge，本身仍为抽象类（不实现 compare）。
// 没有数据成员，只封装了受保护的静态方法 normalizeNewlines()，供下面各种文本比较子类共用。
class TextOutputJudge : public OutputJudge
{
  protected:
    // 把文本 text 里的 Windows 换行 "\r\n" 统一转成 "\n"，其余字符不动，返回转换后的字符串
    static std::string normalizeNewlines(const std::string &text);
};

// 精确比较器，公有继承自 TextOutputJudge，负责具体的严格文本比对。
// 无新增属性，重写 compare() 方法：先把两边换行符统一，再逐字符检查是否完全相同。
class ExactJudge : public TextOutputJudge
{
  public:
    // 重写基类 compare()：统一换行符后比较 actual 与 expected，完全一样才返回 true
    virtual bool compare(const std::string &actual, const std::string &expected) const;
};

#endif