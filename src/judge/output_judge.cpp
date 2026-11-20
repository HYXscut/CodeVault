#include "output_judge.h"

using namespace std;

// 将文本中的 CRLF ("\r\n") 统一替换为 LF ("\n")，其他字符（包括空格和末尾换行）原样保留
string TextOutputJudge::normalizeNewlines(const string &text)
{
    string result;

    for (size_t i = 0; i < text.size(); ++i)
    {
        // 如果当前是 '\r' 且紧接着下一个字符是 '\n'，就合并成一个 '\n'
        if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
        {
            result += '\n';
            ++i; // 跳过已经合并掉的 '\n'
        }
        else
        {
            result += text[i];
        }
    }

    return result;
}

bool ExactJudge::compare(const string &actual, const string &expected) const
{
    // 统一换行符后直接用 string 的 == 比较是否完全一致
    return normalizeNewlines(actual) == normalizeNewlines(expected);
}