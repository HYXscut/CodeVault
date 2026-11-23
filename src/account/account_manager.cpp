#include "account/account_manager.h"
#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QString>
#include <iomanip>
#include <sstream>

namespace
{
bool validUsername(const std::string& text) // 3～20 位 ASCII 字母、数字或下划线，不自动改大小写。
{
    if (text.size() < 3 || text.size() > 20) return false;
    for (char ch : text)
    {
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '_')) return false;
    }
    return true;
}

bool validId(const std::string& text) // 内部 ID 也允许迁移前的用户名，但不允许分隔符或换行。
{
    return validUsername(text);
}

bool hexField(const std::string& text, int bytes) // 固定长度的小写十六进制，避免宽松解码接受坏数据。
{
    if (text.size() != static_cast<std::size_t>(bytes * 2)) return false;
    for (char ch : text)
        if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) return false;
    return true;
}

bool line(std::istream& input, std::string& result) // 逐行读取；兼容 CRLF，不裁剪字段内容。
{
    if (!std::getline(input, result)) return false;
    if (!result.empty() && result.back() == '\r') result.pop_back();
    return true;
}

bool uniqueAccount(const std::vector<Account>& accounts, const Account& current, std::string& error)
{
    for (const Account& old : accounts) // 用户名和 ID 分别查重，禁止两个用户共用同一进度身份。
    {
        if (old.username == current.username || old.userId == current.userId)
        { error = "账号文件存在重复用户名或内部 ID"; return false; }
    }
    return true;
}

QByteArray derive(const std::string& password, const Account& account)
{
    return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,
        QByteArray::fromStdString(password), QByteArray::fromHex(QByteArray::fromStdString(account.saltHex)),
        account.iterations, 32); // Qt 将密码、盐和次数派生为 32 字节验证值。
}

bool protectPassword(const std::string& password, Account& account, std::string& error)
{
    QByteArray salt;
    for (int i = 0; i < 4; ++i)
    {
        const quint32 random = QRandomGenerator::system()->generate(); // 使用系统随机源，不使用时间作种子。
        for (int j = 0; j < 4; ++j) salt.append(static_cast<char>((random >> (j * 8)) & 255));
    }
    account.saltHex = salt.toHex().toStdString();
    const QByteArray verifier = derive(password, account);
    if (verifier.size() != 32) { error = "口令派生失败，未保存账号"; return false; }
    account.verifierHex = verifier.toHex().toStdString();
    return true;
}

bool readAccounts(const QByteArray& bytes, std::vector<Account>& result, bool& legacy, std::string& error)
{
    std::istringstream input(bytes.toStdString());
    std::string header;
    line(input, header);
    legacy = header != "CJACCOUNTS1";
    std::vector<Account> candidate;
    if (!legacy)
    {
        std::string countText;
        if (!line(input, countText) || countText.empty()) { error = "账号记录数缺失"; return false; }
        for (char ch : countText)
            if (ch < '0' || ch > '9') { error = "账号记录数非法"; return false; }
        int count = 0;
        std::istringstream number(countText);
        if (!(number >> count) || count > 1000) { error = "账号记录数应为 0～1000"; return false; }
        for (int i = 0; i < count; ++i)
        {
            Account current;
            std::string iterations;
            if (!line(input, current.userId) || !line(input, current.username) ||
                !line(input, current.algorithm) || !line(input, iterations) ||
                !line(input, current.saltHex) || !line(input, current.verifierHex))
            { error = "账号记录字段不完整"; return false; }
            if (!validId(current.userId) || !validUsername(current.username) ||
                current.algorithm != "PBKDF2-SHA256" || iterations != "600000" ||
                !hexField(current.saltHex, 16) || !hexField(current.verifierHex, 32))
            { error = "账号记录格式或算法参数错误"; return false; }
            if (!uniqueAccount(candidate, current, error)) return false;
            candidate.push_back(current);
        }
        std::string extra;
        if (std::getline(input, extra)) { error = "账号文件有多余内容"; return false; }
    }
    else
    {
        input.clear(); input.seekg(0); // 兼容旧版每行“用户名 密码”，不读取或改写进度文件。
        std::string record;
        while (line(input, record))
        {
            if (record.empty()) continue; // 旧版首次创建的空文件允许迁移。
            std::istringstream fields(record);
            Account current;
            std::string password, extra;
            if (!(fields >> current.username >> password) || (fields >> extra) ||
                !validUsername(current.username) || candidate.size() >= 1000)
            { error = "旧账号文件格式不合法，未转换；请检查备份，不要清空原数据"; return false; }
            current.userId = current.username; // 保留旧 ID，使已有进度无需跨文件迁移。
            if (!uniqueAccount(candidate, current, error)) return false;
            if (!protectPassword(password, current, error)) return false;
            candidate.push_back(current);
        }
    }
    if (input.bad()) { error = "账号内容读取失败"; return false; }
    result = candidate;
    return true;
}
}

AccountManager::AccountManager(std::string filePath) : filePath_(filePath) {}

bool AccountManager::load(std::string& error)
{
    loaded_ = false;
    logout();
    const QString path = QString::fromUtf8(filePath_.c_str());
    const QFileInfo info(path);
    if (filePath_.empty() || !info.isAbsolute()) { error = "账号文件必须使用绝对路径"; return false; }
    std::vector<Account> candidate;
    if (info.exists() || info.isSymLink()) // 存在但不可读，不能当作首次运行去覆盖。
    {
        if (!info.isFile()) { error = "账号路径不是有效文件"; return false; }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
        { error = "无法打开账号文件：" + file.errorString().toUtf8().toStdString(); return false; }
        const QByteArray bytes = file.readAll();
        if (file.error() != QFileDevice::NoError) { error = "账号文件读取失败"; return false; }
        file.close(); // 转换提交前释放读取句柄。
        bool legacy = false;
        if (!readAccounts(bytes, candidate, legacy, error)) return false;
        if (legacy && !save(candidate, error)) return false; // 旧版仅在完整原子转换成功后才启用。
    }
    else if (!save(candidate, error)) return false; // 首次创建带格式标识的空表，也检查目录可写。
    accounts_ = candidate;
    loaded_ = true;
    error.clear();
    return true;
}

bool AccountManager::save(const std::vector<Account>& candidate, std::string& error)
{
    const QString path = QString::fromUtf8(filePath_.c_str());
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) { error = "无法创建账号目录"; return false; }
    std::ostringstream text;
    text << "CJACCOUNTS1\n" << candidate.size() << '\n';
    for (const Account& account : candidate)
        text << account.userId << '\n' << account.username << '\n' << account.algorithm << '\n'
             << account.iterations << '\n' << account.saltHex << '\n' << account.verifierHex << '\n';
    QSaveFile file(path);
    file.setDirectWriteFallback(false); // 不退回直接覆盖，保证失败时旧文件仍在。
    if (!file.open(QIODevice::WriteOnly))
    { error = "无法保存账号文件：" + file.errorString().toUtf8().toStdString(); return false; }
    const QByteArray bytes = QByteArray::fromStdString(text.str());
    if (file.write(bytes) != bytes.size())
    { error = "账号文件写入失败"; file.cancelWriting(); return false; }
    if (!file.commit())
    { error = "账号文件提交失败：" + file.errorString().toUtf8().toStdString(); return false; }
    error.clear();
    return true;
}

bool AccountManager::registerAccount(const std::string& username, const std::string& password, std::string& error)
{
    if (!loaded_) { error = "账号文件尚未成功加载"; return false; }
    if (!validUsername(username)) { error = "用户名须为 3～20 位英文字母、数字或下划线"; return false; }
    if (password.empty()) { error = "密码不能为空"; return false; }
    if (accounts_.size() >= 1000) { error = "账号数量已达上限 1000"; return false; }
    for (const Account& old : accounts_)
        if (old.username == username) { error = "用户名已经存在"; return false; }
    Account account;
    account.username = username;
    for (int number = 1; number <= 1001; ++number)
    {
        std::ostringstream id;
        id << 'u' << std::setw(4) << std::setfill('0') << number;
        bool used = false;
        for (const Account& old : accounts_) if (old.userId == id.str()) used = true;
        if (!used) { account.userId = id.str(); break; } // 检查历史 ID 冲突，不按用户名分配身份。
    }
    if (!protectPassword(password, account, error)) return false;
    std::vector<Account> candidate = accounts_;
    candidate.push_back(account);
    if (!save(candidate, error)) return false;
    accounts_ = candidate;
    error.clear();
    return true;
}

bool AccountManager::login(const std::string& username, const std::string& password,
                           std::string& userId, std::string& error)
{
    if (!loaded_) { error = "账号文件尚未成功加载"; return false; }
    if (isLoggedIn()) { error = "请先退出当前账号再登录"; return false; }
    for (const Account& account : accounts_)
    {
        if (account.username != username) continue;
        const QByteArray actual = derive(password, account);
        const QByteArray expected = QByteArray::fromHex(QByteArray::fromStdString(account.verifierHex));
        if (actual.size() != 32 || expected.size() != 32) break;
        unsigned int difference = 0;
        for (int i = 0; i < 32; ++i)
            difference |= static_cast<unsigned char>(actual[i]) ^ static_cast<unsigned char>(expected[i]);
        if (difference != 0) break; // 比较完整验证值，不在首个不同字节处结束循环。
        currentUserId_ = account.userId;
        userId = currentUserId_;
        error.clear();
        return true;
    }
    error = "用户名或密码错误";
    return false;
}

bool AccountManager::isLoggedIn() const { return !currentUserId_.empty(); }
std::string AccountManager::currentUserId() const { return currentUserId_; }
void AccountManager::logout() { currentUserId_.clear(); }
