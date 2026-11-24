// 输出比较器实现：换行符统一与精确文本比对
#include "output_judge.h"

using namespace std;

// 把字符串 text 中的 "\r\n" 全部换成 "\n"，返回处理后的新字符串。
// 做法：从头到尾扫一遍 text，遇到 '\r' 且紧跟着 '\n' 就写入一个 '\n' 并跳过下一位，
// 空格、空行和末尾换行都按原样保留，不随便删改。
string TextOutputJudge::normalizeNewlines(const string &text)
{
    string result;

    for (size_t i = 0; i < text.size(); ++i)
    {
        // 遇到成对的 "\r\n" 合并成一个 '\n'，并多跳过后面的 '\n'
        if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
        {
            result += '\n';
            ++i;
        }
        else
        {
            result += text[i];
        }
    }

    return result;
}

// 精确比较程序实际输出 actual 和标准答案 expected，完全相同返回 true。
// 做法：先调用父类 TextOutputJudge::normalizeNewlines 消除系统换行差异，再用 == 逐字符比较。
bool ExactJudge::compare(const string &actual, const string &expected) const
{
    return normalizeNewlines(actual) == normalizeNewlines(expected);
}