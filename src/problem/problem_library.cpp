#include "problem/problem_library.h"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace
{

std::string pathToUtf8(const fs::path& path)
{
#if defined(__cpp_char8_t)
    const auto text = path.u8string();
    return std::string(text.begin(), text.end());
#else
    return path.u8string();
#endif
}

void removeUtf8Bom(std::string& text)
{
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF)
    {
        text.erase(0, 3);
    }
}

bool readLines(
    const fs::path& filePath,
    std::vector<std::string>& lines,
    std::string& error)
{
    std::ifstream input(filePath, std::ios::binary);

    if (!input)
    {
        error = "无法打开文件：" + pathToUtf8(filePath);
        return false;
    }

    std::vector<std::string> newLines;
    std::string line;

    while (std::getline(input, line))
    {
        // std::getline 会删除 \n，但 CRLF 中的 \r 仍可能保留。
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        newLines.push_back(line);
    }

    if (input.bad())
    {
        error = "读取文件失败：" + pathToUtf8(filePath);
        return false;
    }

    if (!newLines.empty())
    {
        removeUtf8Bom(newLines[0]);
    }

    lines = std::move(newLines);
    error.clear();
    return true;
}

bool readTextFile(
    const fs::path& filePath,
    std::string& text,
    std::string& error)
{
    std::ifstream input(filePath, std::ios::binary);

    if (!input)
    {
        error = "无法打开题面文件：" + pathToUtf8(filePath);
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    if (input.bad())
    {
        error = "读取题面文件失败：" + pathToUtf8(filePath);
        return false;
    }

    std::string newText = buffer.str();
    removeUtf8Bom(newText);

    text = std::move(newText);
    error.clear();
    return true;
}

bool parseInteger(
    const std::string& text,
    int minValue,
    int maxValue,
    int& result)
{
    if (text.empty())
    {
        return false;
    }

    int value = 0;

    const char* begin = text.data();
    const char* end = begin + text.size();

    const auto conversion =
        std::from_chars(begin, end, value);

    if (conversion.ec != std::errc() ||
        conversion.ptr != end)
    {
        return false;
    }

    if (value < minValue || value > maxValue)
    {
        return false;
    }

    result = value;
    return true;
}

bool isValidId(const std::string& id)
{
    if (id.empty())
    {
        return false;
    }

    for (char ch : id)
    {
        const bool isLetter =
            (ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z');

        const bool isDigit =
            ch >= '0' && ch <= '9';

        if (!isLetter &&
            !isDigit &&
            ch != '_' &&
            ch != '-')
        {
            return false;
        }
    }

    return true;
}

bool splitTags(
    const std::string& text,
    std::vector<std::string>& tags)
{
    std::vector<std::string> newTags;

    if (text.empty())
    {
        tags.clear();
        return true;
    }

    std::size_t start = 0;

    while (true)
    {
        const std::size_t separator =
            text.find(';', start);

        const std::string tag =
            separator == std::string::npos
                ? text.substr(start)
                : text.substr(start, separator - start);

        // 标签字段不能出现连续分号产生的空标签。
        if (tag.empty())
        {
            return false;
        }

        newTags.push_back(tag);

        if (separator == std::string::npos)
        {
            break;
        }

        start = separator + 1;
    }

    tags = std::move(newTags);
    return true;
}

bool isDirectory(
    const fs::path& path,
    std::string& error)
{
    std::error_code ec;
    const bool result = fs::is_directory(path, ec);

    if (ec)
    {
        error = "无法检查目录：" +
                pathToUtf8(path) +
                "，原因：" + ec.message();
        return false;
    }

    if (!result)
    {
        error = "目录不存在或路径不是文件夹：" +
                pathToUtf8(path);
        return false;
    }

    error.clear();
    return true;
}

bool checkReadableFile(
    const fs::path& filePath,
    std::string& error)
{
    std::error_code ec;
    const bool regularFile =
        fs::is_regular_file(filePath, ec);

    if (ec)
    {
        error = "无法检查测试文件：" +
                pathToUtf8(filePath) +
                "，原因：" + ec.message();
        return false;
    }

    if (!regularFile)
    {
        error = "缺少测试文件：" +
                pathToUtf8(filePath);
        return false;
    }

    std::ifstream input(filePath, std::ios::binary);

    if (!input)
    {
        error = "测试文件不可读：" +
                pathToUtf8(filePath);
        return false;
    }

    // 空输入文件或空输出文件也可能是合法测试数据。
    input.peek();

    if (input.bad())
    {
        error = "读取测试文件失败：" +
                pathToUtf8(filePath);
        return false;
    }

    error.clear();
    return true;
}

bool preparePackagesDirectory(
    const fs::path& packagesPath,
    std::string& error)
{
    std::error_code ec;

    const bool exists =
        fs::exists(packagesPath, ec);

    if (ec)
    {
        error = "无法检查题包保存目录：" +
                ec.message();
        return false;
    }

    if (!exists)
    {
        fs::create_directories(packagesPath, ec);

        if (ec)
        {
            error = "无法创建题包保存目录：" +
                    ec.message();
            return false;
        }
    }

    const bool directory =
        fs::is_directory(packagesPath, ec);

    if (ec)
    {
        error = "无法访问题包保存目录：" +
                ec.message();
        return false;
    }

    if (!directory)
    {
        error = "题包保存路径不是文件夹";
        return false;
    }

    error.clear();
    return true;
}

bool checkNoExtraTestFiles(
    const fs::path& problemFolder,
    int testCount,
    std::string& error)
{
    std::error_code ec;

    for (fs::directory_iterator iterator(problemFolder, ec);
         iterator != fs::directory_iterator();
         iterator.increment(ec))
    {
        if (ec)
        {
            error = "遍历题目目录失败：" +
                    ec.message();
            return false;
        }

        const fs::directory_entry& entry = *iterator;

        const bool regularFile =
            entry.is_regular_file(ec);

        if (ec)
        {
            error = "检查题目文件失败：" +
                    ec.message();
            return false;
        }

        if (!regularFile)
        {
            continue;
        }

        const std::string extension =
            pathToUtf8(entry.path().extension());

        if (extension != ".in" &&
            extension != ".out")
        {
            continue;
        }

        const std::string stem =
            pathToUtf8(entry.path().stem());

        int testIndex = 0;

        if (!parseInteger(
                stem,
                1,
                testCount,
                testIndex))
        {
            error = "存在多余或编号错误的测试文件：" +
                    pathToUtf8(entry.path());
            return false;
        }

        const std::string expectedName =
            std::to_string(testIndex) + extension;

        if (pathToUtf8(entry.path().filename()) != expectedName)
        {
            error = "测试文件编号格式错误：" +
                    pathToUtf8(entry.path());
            return false;
        }
    }

    if (ec)
    {
        error = "遍历题目目录失败：" +
                ec.message();
        return false;
    }

    error.clear();
    return true;
}

bool parsePackageFolder(
    const fs::path& packageFolder,
    std::string& parsedPackageId,
    std::vector<Problem>& parsedProblems,
    std::string& error)
{
    if (!isDirectory(packageFolder, error))
    {
        return false;
    }

    const fs::path packageFile =
        packageFolder / "package.txt";

    std::vector<std::string> lines;

    if (!readLines(packageFile, lines, error))
    {
        return false;
    }

    if (lines.size() < 4)
    {
        error = "package.txt 少于四行：" +
                pathToUtf8(packageFile);
        return false;
    }

    if (lines[0] != "CJPKG1")
    {
        error = "题包格式标识必须是 CJPKG1：" +
                pathToUtf8(packageFile);
        return false;
    }

    const std::string packageId = lines[1];

    if (!isValidId(packageId))
    {
        error = "题包编号格式错误：" + packageId;
        return false;
    }

    if (lines[2].empty())
    {
        error = "题包标题不能为空：" +
                pathToUtf8(packageFile);
        return false;
    }

    int problemCount = 0;

    if (!parseInteger(
            lines[3],
            1,
            200,
            problemCount))
    {
        error = "题目数量必须是 1 到 200 的整数：" +
                pathToUtf8(packageFile);
        return false;
    }

    const std::size_t expectedLineCount =
        4 + static_cast<std::size_t>(problemCount) * 5;

    if (lines.size() != expectedLineCount)
    {
        error = "package.txt 的题目记录行数不正确：" +
                pathToUtf8(packageFile);
        return false;
    }

    std::vector<Problem> newProblems;
    std::set<std::string> problemIds;

    for (int index = 0; index < problemCount; ++index)
    {
        const std::size_t offset =
            4 + static_cast<std::size_t>(index) * 5;

        Problem problem;

        problem.packageId = packageId;
        problem.problemId = lines[offset];
        problem.title = lines[offset + 1];

        if (!isValidId(problem.problemId))
        {
            error = "题目编号格式错误：" +
                    problem.problemId;
            return false;
        }

        if (problem.title.empty())
        {
            error = "题目标题不能为空：" +
                    problem.problemId;
            return false;
        }

        if (!problemIds.insert(problem.problemId).second)
        {
            error = "同一题包中的题目编号重复：" +
                    problem.problemId;
            return false;
        }

        if (!splitTags(
                lines[offset + 2],
                problem.tags))
        {
            error = "题目标签格式错误：" +
                    problem.problemId;
            return false;
        }

        if (!parseInteger(
                lines[offset + 3],
                100,
                10000,
                problem.timeLimitMs))
        {
            error = "题目时限必须是 100 到 10000 毫秒：" +
                    problem.problemId;
            return false;
        }

        if (!parseInteger(
                lines[offset + 4],
                1,
                20,
                problem.testCount))
        {
            error = "测试点数量必须是 1 到 20：" +
                    problem.problemId;
            return false;
        }

        const fs::path problemFolder =
            packageFolder / fs::u8path(problem.problemId);

        if (!isDirectory(problemFolder, error))
        {
            error = "题目目录错误：" +
                    problem.problemId +
                    "；" + error;
            return false;
        }

        const fs::path statementFile =
            problemFolder / "statement.md";

        if (!readTextFile(
                statementFile,
                problem.statement,
                error))
        {
            return false;
        }

        for (int testIndex = 1;
             testIndex <= problem.testCount;
             ++testIndex)
        {
            const fs::path inputFile =
                problemFolder /
                (std::to_string(testIndex) + ".in");

            const fs::path outputFile =
                problemFolder /
                (std::to_string(testIndex) + ".out");

            if (!checkReadableFile(inputFile, error))
            {
                return false;
            }

            if (!checkReadableFile(outputFile, error))
            {
                return false;
            }
        }

        if (!checkNoExtraTestFiles(
                problemFolder,
                problem.testCount,
                error))
        {
            return false;
        }

        std::error_code absoluteError;
        const fs::path absoluteProblemFolder =
            fs::absolute(problemFolder, absoluteError);

        if (absoluteError)
        {
            error = "无法取得题目目录的绝对路径：" +
                    absoluteError.message();
            return false;
        }

        problem.problemDir =
            pathToUtf8(
                absoluteProblemFolder.lexically_normal());

        newProblems.push_back(std::move(problem));
    }

    parsedPackageId = packageId;
    parsedProblems = std::move(newProblems);
    error.clear();
    return true;
}

} // namespace

ProblemLibrary::ProblemLibrary(std::string packagesDir)
    : packagesDir_(std::move(packagesDir))
{
}

bool ProblemLibrary::load(std::string& error)
{
    const fs::path packagesPath =
        fs::u8path(packagesDir_);

    if (!preparePackagesDirectory(
            packagesPath,
            error))
    {
        return false;
    }

    std::vector<fs::path> packageFolders;
    std::error_code ec;

    for (fs::directory_iterator iterator(packagesPath, ec);
         iterator != fs::directory_iterator();
         iterator.increment(ec))
    {
        if (ec)
        {
            error = "遍历题包保存目录失败：" +
                    ec.message();
            return false;
        }

        const fs::directory_entry& entry = *iterator;

        const bool directory =
            entry.is_directory(ec);

        if (ec)
        {
            error = "检查题包目录失败：" +
                    ec.message();
            return false;
        }

        if (!directory)
        {
            continue;
        }

        const std::string folderName =
            pathToUtf8(entry.path().filename());

        // 导入中断后可能残留临时目录，不将其当作正式题包。
        if (folderName.rfind(".importing-", 0) == 0)
        {
            continue;
        }

        packageFolders.push_back(entry.path());
    }

    if (ec)
    {
        error = "遍历题包保存目录失败：" +
                ec.message();
        return false;
    }

    std::sort(
        packageFolders.begin(),
        packageFolders.end(),
        [](const fs::path& left, const fs::path& right)
        {
            return pathToUtf8(left.filename()) <
                   pathToUtf8(right.filename());
        });

    std::vector<Problem> loadedProblems;
    std::set<std::string> loadedPackageIds;

    for (const fs::path& packageFolder : packageFolders)
    {
        std::string packageId;
        std::vector<Problem> packageProblems;

        if (!parsePackageFolder(
                packageFolder,
                packageId,
                packageProblems,
                error))
        {
            return false;
        }

        if (pathToUtf8(packageFolder.filename()) != packageId)
        {
            error = "保存的题包目录名必须与题包编号一致：" +
                    packageId;
            return false;
        }

        if (!loadedPackageIds.insert(packageId).second)
        {
            error = "题包编号重复：" + packageId;
            return false;
        }

        loadedProblems.insert(
            loadedProblems.end(),
            packageProblems.begin(),
            packageProblems.end());
    }

    // 所有题包均成功后，才替换原题库。
    problems_ = std::move(loadedProblems);
    loaded_ = true;

    error.clear();
    return true;
}

bool ProblemLibrary::importPackage(
    std::string folder,
    std::string& error)
{
    if (!loaded_)
    {
        error = "题库尚未成功加载，不能导入题包";
        return false;
    }

    const fs::path sourceFolder =
        fs::u8path(folder);

    std::string packageId;
    std::vector<Problem> sourceProblems;

    // 复制前先验证外部题包，避免复制明显错误的数据。
    if (!parsePackageFolder(
            sourceFolder,
            packageId,
            sourceProblems,
            error))
    {
        return false;
    }

    for (const Problem& problem : problems_)
    {
        if (problem.packageId == packageId)
        {
            error = "题包编号已经存在：" + packageId;
            return false;
        }
    }

    const fs::path packagesPath =
        fs::u8path(packagesDir_);

    if (!preparePackagesDirectory(
            packagesPath,
            error))
    {
        return false;
    }

    const fs::path destinationFolder =
        packagesPath / fs::u8path(packageId);

    std::error_code ec;
    const bool destinationExists =
        fs::exists(destinationFolder, ec);

    if (ec)
    {
        error = "无法检查目标题包目录：" +
                ec.message();
        return false;
    }

    if (destinationExists)
    {
        error = "题包保存目录已经存在：" +
                pathToUtf8(destinationFolder);
        return false;
    }

    fs::path temporaryFolder;

    for (int index = 0; index < 1000; ++index)
    {
        const fs::path candidate =
            packagesPath /
            fs::u8path(
                ".importing-" +
                packageId +
                "-" +
                std::to_string(index));

        const bool candidateExists =
            fs::exists(candidate, ec);

        if (ec)
        {
            error = "无法准备导入临时目录：" +
                    ec.message();
            return false;
        }

        if (!candidateExists)
        {
            temporaryFolder = candidate;
            break;
        }
    }

    if (temporaryFolder.empty())
    {
        error = "无法取得可用的导入临时目录";
        return false;
    }

    fs::copy(
        sourceFolder,
        temporaryFolder,
        fs::copy_options::recursive,
        ec);

    if (ec)
    {
        std::error_code cleanupError;
        fs::remove_all(
            temporaryFolder,
            cleanupError);

        error = "复制题包失败：" + ec.message();
        return false;
    }

    // 再验证一次副本，确保复制后的文件完整。
    std::string copiedPackageId;
    std::vector<Problem> copiedProblems;

    if (!parsePackageFolder(
            temporaryFolder,
            copiedPackageId,
            copiedProblems,
            error))
    {
        std::error_code cleanupError;
        fs::remove_all(
            temporaryFolder,
            cleanupError);

        return false;
    }

    if (copiedPackageId != packageId)
    {
        std::error_code cleanupError;
        fs::remove_all(
            temporaryFolder,
            cleanupError);

        error = "复制后的题包编号发生变化";
        return false;
    }

    fs::rename(
        temporaryFolder,
        destinationFolder,
        ec);

    if (ec)
    {
        std::error_code cleanupError;
        fs::remove_all(
            temporaryFolder,
            cleanupError);

        error = "保存题包副本失败：" +
                ec.message();
        return false;
    }

    // 使用最终保存位置重新解析，以生成正确的 problemDir。
    std::string savedPackageId;
    std::vector<Problem> savedProblems;

    if (!parsePackageFolder(
            destinationFolder,
            savedPackageId,
            savedProblems,
            error))
    {
        std::error_code cleanupError;
        fs::remove_all(
            destinationFolder,
            cleanupError);

        return false;
    }

    std::vector<Problem> updatedProblems = problems_;

    updatedProblems.insert(
        updatedProblems.end(),
        savedProblems.begin(),
        savedProblems.end());

    problems_ = std::move(updatedProblems);

    error.clear();
    return true;
}

std::vector<Problem> ProblemLibrary::getProblems()
{
    return problems_;
}

bool ProblemLibrary::findProblem(
    std::string packageId,
    std::string problemId,
    Problem& result,
    std::string& error)
{
    for (const Problem& problem : problems_)
    {
        if (problem.packageId == packageId &&
            problem.problemId == problemId)
        {
            result = problem;
            error.clear();
            return true;
        }
    }

    error = "未找到题目：" +
            packageId +
            "/" +
            problemId;

    return false;
}