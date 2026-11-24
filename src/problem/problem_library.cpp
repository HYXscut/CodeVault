#include "problem/problem_library.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QString>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace
{
// 以下函数用于把文件路径转为 UTF-8 字符串
// path为待转换路径；返回转换后的文字。
// 逻辑：读取路径的UTF-8表示，兼容C++17和C++20的字符类型差异。
std::string pathToUtf8(const fs::path& path)
{
#if defined(__cpp_char8_t)
    const auto text = path.u8string();
    return std::string(text.begin(), text.end());
#else
    return path.u8string();
#endif
}

// 以下函数用于去掉文本开头的UTF-8编码标记
// text为需要处理的文本，直接修改原字符串。
// 逻辑：检查前三个字节，有标记才删除；其他内容不变。
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

// 以下函数用于按行读取题包信息
// filePath为文件路径；lines返回各行内容，error返回错误原因。
// 逻辑：逐行读取，去掉行末回车和首行编码标记，全部成功后再赋值。
// 成功返回 true；失败返回 false，不修改 lines。
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

    lines = newLines; // 全部读完后才修改结果
    error.clear();
    return true;
}

// 以下函数用于读取题面
// filePath为题面路径；text返回内容，error返回错误原因。
// 逻辑：读完文件后去掉开头的编码标记，再返回题面。
// 成功返回 true；失败返回 false，不修改 text。
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

    text = newText;
    error.clear();
    return true;
}

// 以下函数用于读取指定范围内的整数
// text为数字文本，minValue和maxValue为范围；result返回整数。
// 逻辑：检查字符，再用字符串流读取，最后判断范围。
// 成功返回 true；格式错误或越界返回 false，不修改 result。
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

    std::size_t start = 0;
    if (text[0] == '-') start = 1; // 保留原来允许负号、不允许加号的规则
    if (start == text.size()) return false;
    for (std::size_t i = start; i < text.size(); ++i)
    {
        if (text[i] < '0' || text[i] > '9') return false;
    }

    std::istringstream input(text);
    if (!(input >> value)) return false; // 超出 int 范围也算失败

    if (value < minValue || value > maxValue)
    {
        return false;
    }

    result = value;
    return true;
}

// 以下函数用于检查题包或题目编号
// id为待检查编号；合法返回 true，否则返回 false。
// 逻辑：编号不能为空，只允许英文字母、数字、下划线和连字符。
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

// 以下函数用于拆分标签
// text为分号分隔的文本；tags返回标签列表。
// 逻辑：按分号逐段提取，空文本表示没有标签，连续或末尾分号视为错误。
// 成功返回 true；失败返回 false，不修改 tags。
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

    tags = newTags;
    return true;
}

// 以下函数用于检查目录是否存在
// path为目录路径，error返回错误原因。
// 逻辑：查询路径类型，是目录返回 true，检查失败或不是目录返回 false。
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

// 以下函数用于检查测试文件能否读取
// filePath为文件路径，error返回错误原因。
// 逻辑：检查文件类型，再打开并尝试读取；空文件也允许。
// 检查通过返回 true，否则返回 false。
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

// 以下函数用于准备题包保存目录
// packagesPath为保存目录，error返回错误原因。
// 逻辑：目录不存在就创建，再确认路径确实是目录。
// 成功返回 true，否则返回 false。
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

// 以下函数用于检查多余或编号错误的测试文件
// problemFolder为题目目录，testCount为测试点总数，error返回错误原因。
// 逻辑：遍历.in和.out文件，编号须在1到testCount之间，且不能写成01等形式。
// 其他扩展名和子目录不参与检查；通过返回 true，否则返回 false。
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

// 以下函数用于检查并读取整个题包
// packageFolder为题包目录；parsedPackageId和parsedProblems返回编号与题目，error返回错误原因。
// 逻辑：检查文件头和每题五行信息，再检查题面及测试文件，计算保存路径。
// 全部成功返回 true；失败返回 false，不修改两个结果参数。
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
        4 + static_cast<std::size_t>(problemCount) * 5; // 文件头四行，每题五行

    if (lines.size() != expectedLineCount)
    {
        error = "package.txt 的题目记录行数不正确：" +
                pathToUtf8(packageFile);
        return false;
    }

    std::vector<Problem> newProblems;
    std::vector<std::string> problemIds;

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

        for (std::size_t i = 0; i < problemIds.size(); ++i)
        {
            if (problemIds[i] == problem.problemId)
            {
                error = "同一题包中的题目编号重复：" + problem.problemId;
                return false;
            }
        }
        problemIds.push_back(problem.problemId);

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

        newProblems.push_back(problem);
    }

    parsedPackageId = packageId;
    parsedProblems = newProblems; // 整个题包检查通过后再返回题目
    error.clear();
    return true;
}

}

// 以下构造函数用于记录题包保存目录
// packagesDir为UTF-8目录路径；这里只保存路径，不读取文件。
ProblemLibrary::ProblemLibrary(std::string packagesDir)
    : packagesDir_(packagesDir)
{
}

// 以下函数用于恢复已保存的题库
// error返回错误原因。
// 逻辑：读取并排序题包，检查编号和隐藏标记，全部成功后更新题库。
// 成功返回 true；失败返回 false，保留旧题目并禁止继续修改题库。
bool ProblemLibrary::load(std::string& error)
{
    loaded_ = false; // 本次加载失败后禁止导入或移除
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

    // 按目录名作选择排序，保持每次启动的题目顺序一致
    for (std::size_t i = 0; i < packageFolders.size(); ++i)
    {
        std::size_t minIndex = i;
        for (std::size_t j = i + 1; j < packageFolders.size(); ++j)
        {
            if (pathToUtf8(packageFolders[j].filename()) <
                pathToUtf8(packageFolders[minIndex].filename())) minIndex = j;
        }
        if (minIndex != i)
        {
            fs::path temporary = packageFolders[i];
            packageFolders[i] = packageFolders[minIndex];
            packageFolders[minIndex] = temporary;
        }
    }

    std::vector<Problem> loadedProblems;
    std::vector<std::string> loadedPackageIds;

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

        for (std::size_t i = 0; i < loadedPackageIds.size(); ++i)
        {
            if (loadedPackageIds[i] == packageId)
            {
                error = "题包编号重复：" + packageId;
                return false;
            }
        }
        loadedPackageIds.push_back(packageId);

        loadedProblems.insert(
            loadedProblems.end(),
            packageProblems.begin(),
            packageProblems.end());
    }

    std::vector<std::string> removed;
    const fs::path markerPath = packagesPath.parent_path() / "removed_packages.txt";
    QFile marker(QString::fromUtf8(pathToUtf8(markerPath).c_str()));
    if (QFileInfo::exists(marker.fileName()))
    {
        if (!marker.open(QIODevice::ReadOnly))
        {
            error = "无法读取题包隐藏标记：" + marker.errorString().toUtf8().toStdString();
            return false;
        }
        const QByteArray bytes = marker.readAll();
        if (marker.error() != QFileDevice::NoError)
        {
            error = "读取题包隐藏标记失败";
            return false;
        }
        std::istringstream input(bytes.toStdString());
        std::string id;
        while (std::getline(input, id))
        {
            if (!id.empty() && id.back() == '\r') id.pop_back();
            if (id.empty()) continue;
            bool found = false;
            for (std::size_t i = 0; i < loadedPackageIds.size(); ++i)
            {
                if (loadedPackageIds[i] == id) { found = true; break; }
            }
            if (!isValidId(id) || !found)
            {
                error = "隐藏标记含非法或未知题包编号：" + id;
                return false;
            }
            for (const std::string& old : removed)
            {
                if (old == id) { error = "隐藏标记编号重复：" + id; return false; }
            }
            removed.push_back(id);
        }
    }
    removedPackageIds_ = removed;

    problems_ = loadedProblems;
    loaded_ = true;

    error.clear();
    return true;
}

// 以下函数用于导入题包或恢复已隐藏的题包
// folder为外部题包目录，error返回错误原因。
// 逻辑：新题包先复制到临时目录，验证后改名并加入题库；隐藏题包须与缓存内容相同才恢复。
// 成功返回 true；失败返回 false，不更新内存题库，清理失败时可能残留目录。
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

    if (!parsePackageFolder(
            sourceFolder,
            packageId,
            sourceProblems,
            error))
    {
        return false;
    }

    if (isRemoved(packageId))
    {
        const fs::path cached = fs::u8path(packagesDir_) / fs::u8path(packageId);
        std::string cachedId;
        std::vector<Problem> cachedProblems;
        if (!parsePackageFolder(cached, cachedId, cachedProblems, error)) return false;
        std::vector<std::string> files;
        files.push_back("package.txt");
        for (const Problem& problem : sourceProblems)
        {
            files.push_back(problem.problemId + "/statement.md");
            for (int i = 1; i <= problem.testCount; ++i)
            {
                files.push_back(problem.problemId + "/" + std::to_string(i) + ".in");
                files.push_back(problem.problemId + "/" + std::to_string(i) + ".out");
            }
        }
        for (const std::string& name : files)
        {
            QFile incoming(QString::fromUtf8(pathToUtf8(sourceFolder / fs::u8path(name)).c_str()));
            QFile original(QString::fromUtf8(pathToUtf8(cached / fs::u8path(name)).c_str()));
            if (!incoming.open(QIODevice::ReadOnly) || !original.open(QIODevice::ReadOnly))
            {
                error = "恢复题包时无法读取文件：" + name;
                return false;
            }
            while (!incoming.atEnd() || !original.atEnd())
            {
                const QByteArray a = incoming.read(65536);
                const QByteArray b = original.read(65536); // 分块比较，空格、换行和编码标记也必须相同
                if (incoming.error() != QFileDevice::NoError || original.error() != QFileDevice::NoError)
                { error = "恢复题包时读取失败：" + name; return false; }
                if (a != b)
                { error = "同 ID 题包内容有变化，请使用新 ID：" + name; return false; }
            }
        }
        std::vector<std::string> candidate;
        for (const std::string& id : removedPackageIds_)
            if (id != packageId) candidate.push_back(id);
        if (!saveRemoved(candidate, error)) return false;
        removedPackageIds_ = candidate; // 保存成功后取消隐藏，保留各账号进度
        error.clear();
        return true;
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

    // 先复制到临时目录，检查完整后才改名为正式题包
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

    std::string savedPackageId; // 从最终目录重新解析，避免测试路径仍指向临时目录
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

    problems_ = updatedProblems; // 文件保存和解析成功后才更新题库

    error.clear();
    return true;
}

// 以下函数用于取得可见题目
// 无参数，返回题目列表副本。
// 逻辑：检查加载状态，再排除隐藏题包；未加载时返回空列表。
std::vector<Problem> ProblemLibrary::getProblems()
{
    std::vector<Problem> visible;
    if (!loaded_) return visible;
    for (const Problem& problem : problems_)
        if (!isRemoved(problem.packageId)) visible.push_back(problem);
    return visible;
}

// 以下函数用于按编号查找可见题目
// packageId和problemId确定题目；result返回题目，error返回错误原因。
// 逻辑：检查加载状态，再遍历查找，跳过隐藏题包。
// 找到返回 true；失败返回 false，不修改 result。
bool ProblemLibrary::findProblem(
    std::string packageId,
    std::string problemId,
    Problem& result,
    std::string& error)
{
    if (!loaded_) { error = "题库未成功加载"; return false; }
    for (const Problem& problem : problems_)
    {
        if (!isRemoved(problem.packageId) && problem.packageId == packageId &&
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

// 以下函数用于读取指定测试点
// packageId和problemId确定题目，testIndex为测试点编号；result返回数据，error返回错误原因。
// 逻辑：先找题并检查编号，再读取对应.in和.out，两个文件都读取成功后才赋值。
// 成功返回 true；失败返回 false，不修改 result。
bool ProblemLibrary::getTestCase(
    std::string packageId,
    std::string problemId,
    int testIndex,
    TestCase& result,
    std::string& error
) {
    Problem problem;
    if (!findProblem(packageId, problemId, problem, error)) {
        return false;
    }

    if (problem.testCount < 1 || problem.testCount > 20) {
        error = "该题测试点数量无效，应为 1 到 20";
        return false;
    }

    if (testIndex < 1 || testIndex > problem.testCount) {
        error = "测试点编号超出范围，应为 1 到该题测试点总数";
        return false;
    }

    std::ostringstream numberStream;
    numberStream << testIndex;
    const std::string number = numberStream.str();

    const fs::path inputPath =
        fs::u8path(problem.problemDir) / (number + ".in");

    if (!checkReadableFile(inputPath, error)) {
        return false;
    }

    std::ifstream inputFile(inputPath, std::ios::binary); // 保留测试数据原有字节
    if (!inputFile) {
        error = "无法打开测试输入文件：" + pathToUtf8(inputPath);
        return false;
    }

    std::ostringstream inputBuffer;
    inputBuffer << inputFile.rdbuf();

    if (inputFile.bad()) {
        error = "读取测试输入文件失败：" + pathToUtf8(inputPath);
        return false;
    }

    const std::string inputText = inputBuffer.str();

    const fs::path outputPath =
        fs::u8path(problem.problemDir) / (number + ".out");

    if (!checkReadableFile(outputPath, error)) {
        return false;
    }

    std::ifstream outputFile(outputPath, std::ios::binary);
    if (!outputFile) {
        error = "无法打开测试输出文件：" + pathToUtf8(outputPath);
        return false;
    }

    std::ostringstream outputBuffer;
    outputBuffer << outputFile.rdbuf();

    if (outputFile.bad()) {
        error = "读取测试输出文件失败：" + pathToUtf8(outputPath);
        return false;
    }

    const std::string outputText = outputBuffer.str();

    TestCase completeCase;
    completeCase.index = testIndex;
    completeCase.input = inputText;
    completeCase.expectedOutput = outputText;

    result = completeCase; // 输入和标准输出都读完后再赋值
    error.clear();
    return true;
}

// 以下函数用于判断题包是否已隐藏
// packageId为题包编号。
// 逻辑：遍历隐藏编号，找到返回 true，否则返回 false。
bool ProblemLibrary::isRemoved(const std::string& packageId) const
{
    for (const std::string& id : removedPackageIds_)
        if (id == packageId) return true;
    return false;
}

// 以下函数用于保存题包隐藏标记
// candidate为准备保存的编号列表，error返回错误原因。
// 逻辑：将编号逐行写入临时文件，写完后再替换原文件。
// 成功返回 true；失败返回 false，保留旧文件。
bool ProblemLibrary::saveRemoved(const std::vector<std::string>& candidate, std::string& error)
{
    const fs::path path = fs::u8path(packagesDir_).parent_path() / "removed_packages.txt";
    QSaveFile output(QString::fromUtf8(pathToUtf8(path).c_str()));
    output.setDirectWriteFallback(false); // 禁止直接覆盖旧文件
    if (!output.open(QIODevice::WriteOnly))
    { error = "无法保存隐藏标记：" + output.errorString().toUtf8().toStdString(); return false; }
    std::string text;
    for (const std::string& id : candidate) text += id + "\n";
    const QByteArray bytes = QByteArray::fromStdString(text);
    if (output.write(bytes) != bytes.size())
    { error = "隐藏标记写入失败"; output.cancelWriting(); return false; }
    if (!output.commit())
    { error = "隐藏标记提交失败：" + output.errorString().toUtf8().toStdString(); return false; }
    error.clear();
    return true;
}

// 以下函数用于从列表中移除题包
// packageId为题包编号，error返回错误原因。
// 逻辑：检查题包，在副本中添加隐藏编号，保存成功后才更新内存；缓存和个人进度保留。
// 成功返回 true，否则返回 false。
bool ProblemLibrary::removePackageFromLibrary(std::string packageId, std::string& error)
{
    if (!loaded_) { error = "题库未成功加载"; return false; }
    bool found = false;
    for (const Problem& problem : problems_)
        if (problem.packageId == packageId) found = true;
    if (!found || isRemoved(packageId)) { error = "题包不存在或已经隐藏：" + packageId; return false; }
    std::vector<std::string> candidate = removedPackageIds_;
    candidate.push_back(packageId);
    if (!saveRemoved(candidate, error)) return false;
    removedPackageIds_ = candidate; // 保存成功后才更新隐藏状态
    return true;
}

// 以下函数用于组合筛选可见题目
// query为筛选条件，currentUserProgress须为当前用户的进度；返回匹配题目的副本。
// 逻辑：逐题判断题包、标题、标签和状态，所有条件都满足才加入结果。
// 标题区分大小写并按包含关系匹配，标签精确匹配；没有进度时按未开始处理。
std::vector<Problem> ProblemLibrary::searchProblems(const ProblemQuery& query,
    const std::vector<Progress>& currentUserProgress)
{
    std::vector<Problem> result;
    if (!loaded_) return result;
    for (const Problem& problem : problems_)
    {
        if (isRemoved(problem.packageId)) continue;
        if (!query.packageId.empty() && query.packageId != problem.packageId) continue;
        if (!query.titleKeyword.empty() && problem.title.find(query.titleKeyword) == std::string::npos) continue;
        bool tagMatches = query.tag.empty();
        for (const std::string& tag : problem.tags)
            if (tag == query.tag) tagMatches = true;
        if (!tagMatches) continue;
        int status = ProgressNotStarted; // 没有记录时按未开始处理
        for (const Progress& progress : currentUserProgress)
        {
            if (progress.packageId == problem.packageId && progress.problemId == problem.problemId)
            { status = progress.status; break; }
        }
        if (query.status != -1 && query.status != status) continue;
        result.push_back(problem);
    }
    return result;
}
