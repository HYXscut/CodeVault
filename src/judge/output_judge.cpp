#include "output_judge.h"

std::string TextOutputJudge::normalizeNewlines(const std::string &text)
{
    std::string result;

    for (size_t i = 0; i < text.size(); ++i)
    {
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

bool ExactJudge::compare(const std::string &actual, const std::string &expected) const
{
    return normalizeNewlines(actual)==normalizeNewlines(expected);
}