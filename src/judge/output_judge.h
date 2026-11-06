#ifndef OUTPUT_JUDGE_H
#define OUTPUT_JUDGE_H

#include <string>

class OutputJudge
{
  public:
    virtual ~OutputJudge() = default;

    virtual bool compare(const std::string &actual, const std::string &expected) const = 0;
};

class TextOutputJudge : public OutputJudge
{
  protected:
    static std::string normalizeNewlines(const std::string &text);
};

class ExactJudge : public TextOutputJudge
{
  public:
    bool compare(const std::string &actual, const std::string &expected) const override;
};

#endif