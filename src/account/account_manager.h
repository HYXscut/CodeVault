#ifndef CODEVAULT_ACCOUNT_MANAGER_H
#define CODEVAULT_ACCOUNT_MANAGER_H

#include <string>
#include <vector>

struct Account // 账号持久化记录；不保存明文密码，进度仅引用 userId。
{
    std::string userId; // 稳定内部 ID；新账号分配 u0001 等，旧账号迁移保留原 ID。
    std::string username; // 登录及显示用的用户名，与 ID 分开。
    std::string algorithm = "PBKDF2-SHA256"; // 文件中明确记录派生算法。
    int iterations = 600000; // 版本 1 固定迭代次数。
    std::string saltHex; // 每账号 16 字节随机盐，按十六进制写入。
    std::string verifierHex; // 32 字节派生值，按十六进制写入。
};

class AccountManager // 封装注册、验证和原子文件保存；无继承关系。
{
public:
    explicit AccountManager(std::string filePath); // 记录 UTF-8 绝对路径，不在构造时读写。
    bool load(std::string& error);
    // 整份校验后启用账号；旧版两字段文件原子转换，失败保留文件并禁止操作。
    bool registerAccount(const std::string& username, const std::string& password, std::string& error);
    // 校验用户名及查重，生成候选记录；保存成功才加入正式内存。
    bool login(const std::string& username, const std::string& password,
               std::string& userId, std::string& error);
    // 根据记录的盐重新派生并比较；成功才设置会话及输出 ID，失败不改输出参数。
    bool isLoggedIn() const; // 根据会话 ID 判断登录状态。
    std::string currentUserId() const; // 返回会话 ID 的副本。
    void logout(); // 清空会话，不删除账号和进度。

private:
    std::string filePath_; // 账号文件路径。
    std::vector<Account> accounts_; // 已校验的账号记录。
    std::string currentUserId_; // 当前会话身份。
    bool loaded_ = false; // 最近一次 load 是否成功。
    bool save(const std::vector<Account>& candidate, std::string& error);
    // 将候选集合写入临时文件并提交，失败不修改旧文件和正式内存。
};
#endif
