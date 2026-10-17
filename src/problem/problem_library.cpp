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
// 该函数用于将文件系统路径转为用于错误信息和 Problem::problemDir 的 UTF-8 字符串。
// 参数 path：待转换的路径；返回值：UTF-8 编码的路径文本。
// 实现：取得 u8string()；在 char8_t 可用时逐字符转为 std::string，可以兼容 C++17 和 C++20 对 u8string() 返回类型的差异。
#if defined(__cpp_char8_t)
    const auto text = path.u8string();
    return std::string(text.begin(), text.end());
#else
    return path.u8string();
#endif
}

void removeUtf8Bom(std::string& text)
{
// 该函数用于删除文本开头可选的 UTF-8 BOM。
// 参数 text：输入兼输出的文本；不足三个字节或没有 BOM 时保持原样。
// 实现：比较开头三个字节是否依次为 EF BB BF，匹配后删除这三个字节。
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
// 该函数用于按行读取题包元数据文件。
// 参数 filePath：文件路径；lines：成功时接收所有行；error：接收错误原因。
// 返回值：完整读取返回 true，否则返回 false；失败时不替换 lines。
// 实现：以二进制方式打开文件，逐行读取；去掉 CRLF 留下的 '\r'；检查读取错误，移除首行 BOM，最后才将临时行集合交给 lines。
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
    }// bad() 表示底层读取出错；正常读到文件末尾不算读取失败。

    if (!newLines.empty())
    {
        removeUtf8Bom(newLines[0]);
    }

    lines = std::move(newLines);// 文件完整读取成功后才替换输出参数，失败时不会留下部分行。
    error.clear();
    return true;
}

bool readTextFile(
    const fs::path& filePath,
    std::string& text,
    std::string& error)
{
// 该函数用于完整读取题面文件。
// 参数 filePath：题面路径；text：成功时接收题面；error：接收错误原因。
// 返回值：完整读取返回 true，否则返回 false；失败时不替换 text。
// 实现：将文件缓冲区写入字符串流；确认读取成功后移除开头的 UTF-8 BOM，最后才把临时文本交给 text。
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
    }// bad() 用于区分实际读取错误与正常到达文件末尾。

    std::string newText = buffer.str();
    removeUtf8Bom(newText);

    text = std::move(newText);// 完整读取并处理 BOM 后才将题面交给输出参数。
    error.clear();
    return true;
}

bool parseInteger(
    const std::string& text,
    int minValue,
    int maxValue,
    int& result)
{
// 该函数用于解析并检查指定范围内的十进制整数。
// 参数 text：待解析文本；minValue/maxValue：允许范围的两端；
// result：成功时接收解析值，失败时保持原值。
// 返回值：文本完整表示一个范围内的整数时返回 true，否则返回 false。
// 实现：from_chars() 不跳过空白；同时检查转换错误、是否读到文本末尾以及数值范围，全部通过后才写入 result。
    if (text.empty())
    {
        return false;// 空字符串不是有效整数
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
    }// 除转换成功外，还要求读完全部字符，拒绝带有多余字符的整数。

    if (value < minValue || value > maxValue)
    {
        return false;
    }

    result = value;
    return true;
}

bool isValidId(const std::string& id)
{
// 该函数用于检查题包或题目编号是否符合命名规则。
// 参数 id：待检查的编号；返回值：合法返回 true，否则返回 false。
// 实现：拒绝空串，再逐字符检查；只允许英文字母、数字、下划线和连字符。
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
// 该函数用于将分号分隔的标签字段拆成标签列表。
// 参数 text：原始标签字段；tags：成功时接收标签，失败时保持原值。
// 返回值：格式合法返回 true，否则返回 false。
// 实现：空字段表示没有标签；非空字段按分号逐段提取，若产生空标签（例如连续分号或结尾分号）则拒绝。
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

    tags = std::move(newTags);// 所有标签都通过检查后，才替换调用者原有的标签列表。
    return true;
}

bool isDirectory(
    const fs::path& path,
    std::string& error)
{
// 该函数用于确认路径存在且为目录。
// 参数 path：待检查路径；error：失败时接收文件系统错误或路径错误。
// 返回值：是目录返回 true，否则返回 false。
// 实现：使用带 error_code 的文件系统接口，分别处理检查失败与类型不符。
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
// 该函数用于确认一个测试输入或输出文件存在、是普通文件且可读。
// 参数 filePath：测试文件路径；error：失败时接收原因。
// 返回值：检查通过返回 true，否则返回 false。
// 实现：先检查文件类型，再以二进制方式打开并尝试读取；文件为空是合法情况，因此不把到达文件末尾视为错误。
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
// 该函数用于准备应用保存题包副本的目录。
// 参数 packagesPath：保存目录路径；error：失败时接收原因。
// 返回值：最终路径是可访问的目录时返回 true，否则返回 false。
// 实现：先判断路径是否存在；不存在则递归创建；最后确认它确实是目录。
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
// 该函数用于检查题目目录中是否有多余或编号错误的测试文件。
// 参数 problemFolder：题目目录；testCount：允许的测试点编号上限；error：失败时接收原因。
// 返回值：检查通过返回 true，否则返回 false。
// 实现：遍历目录中的普通文件，只检查 .in 和 .out 文件；文件名主体必须是 1 到 testCount 的规范十进制编号。
// 其他扩展名的文件及子目录不在本函数的检查范围内。
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
        }// 文件名主体必须能解析为 1 到 testCount 之间的编号。

        const std::string expectedName =
            std::to_string(testIndex) + extension;

        if (pathToUtf8(entry.path().filename()) != expectedName)
        {
            error = "测试文件编号格式错误：" +
                    pathToUtf8(entry.path());
            return false;
        }// 比较规范文件名，拒绝 01.in 等数值有效但命名格式不符的文件。
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
// 该函数用于验证并解析一个完整题包。
// 参数 packageFolder：题包根目录；parsedPackageId：成功时接收题包编号；parsedProblems：成功时接收全部题目；error：失败时接收原因。
// 返回值：元数据、题面及测试文件全部符合要求时返回 true，否则返回 false。
// 步骤：验证目录和 package.txt；检查格式标识、编号与题目数量；按每题五行解析元数据；检查题目目录、题面和测试文件；计算题目目录绝对路径；全部成功后才提交两个输出参数。
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
        4 + static_cast<std::size_t>(problemCount) * 5;// 文件头占四行，每道题占五行，据此计算 package.txt 应有的总行数。

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
            4 + static_cast<std::size_t>(index) * 5;// 前四行是题包信息；每道题占五行，offset 指向当前题目的第一行。

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
        }// set::insert 的 second 为 false 时，说明该题目编号已经出现过。

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
        }// 从 1 到 testCount 逐一检查每组输入、输出文件是否存在且可读。

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
                absoluteProblemFolder.lexically_normal());// 保存规范化后的绝对路径，供后续访问该题目的测试文件。

        newProblems.push_back(std::move(problem));
    }

    parsedPackageId = packageId;
    parsedProblems = std::move(newProblems);// 保存规范化后的绝对路径，供后续访问该题目的测试文件。
    error.clear();
    return true;
}

} // namespace

ProblemLibrary::ProblemLibrary(std::string packagesDir)
    : packagesDir_(std::move(packagesDir))
{
// 初始化题库的保存目录。
// 参数 packagesDir：应用保存题包副本的目录路径。
// 实现：使用 std::move() 初始化 packagesDir_，避免不必要的字符串复制；problems_ 保持为空，loaded_ 保持默认值 false。
}

bool ProblemLibrary::load(std::string& error)
{
// 从应用保存目录重新建立题库。
// 参数 error：失败时输出原因，成功时清空。
// 返回值：全部正式题包加载成功返回 true，否则返回 false。
// 步骤：准备目录；收集正式题包目录并排序；逐个解析并检查编号；将题目先放入局部集合，全部通过后才替换 problems_。
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
        });// 按目录名排序，使多次加载得到稳定的题目顺序。

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
// 将外部题包复制到应用目录并加入当前题库。
// 参数 folder：外部题包根目录；error：失败时输出原因，成功时清空。
// 返回值：复制、验证和题库更新全部成功返回 true，否则返回 false。
// 步骤：检查加载状态；验证源题包及编号；选择临时目录并复制；验证副本；重命名到正式目录；重新解析最终位置；更新题目集合。
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
        ec);// 先递归复制到临时目录，复制完成前不占用正式题包目录。

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
        ec);// 将验证通过的临时目录改名为正式题包目录。

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

    problems_ = std::move(updatedProblems);// 文件保存和最终解析均成功后，才更新内存中的题库。

    error.clear();
    return true;
}

std::vector<Problem> ProblemLibrary::getProblems()
{
// 返回当前全部题目的副本。
// 参数：无；返回值：题目 vector 的副本。
// 实现：直接返回 problems_，由 vector 的复制操作构造返回值。
    return problems_;
}

bool ProblemLibrary::findProblem(
    std::string packageId,
    std::string problemId,
    Problem& result,
    std::string& error)
{
// 按“题包编号 + 题目编号”定位一题。
// 参数 packageId/problemId：目标的两个编号；result：成功时输出题目副本；error：失败时输出原因，成功时清空。
// 返回值：找到返回 true，未找到返回 false。
// 实现：顺序遍历题目集合；两个编号同时匹配时才写入 result。
    for (const Problem& problem : problems_)
    {
        if (problem.packageId == packageId &&
            problem.problemId == problemId)
        {
            result = problem;// 只有两个编号同时匹配才修改 result；查找失败时保留其原值。
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