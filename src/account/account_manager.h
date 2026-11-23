#ifndef CODEVAULT_ACCOUNT_MANAGER_H
#define CODEVAULT_ACCOUNT_MANAGER_H

#include <string>
#include <vector>

class AccountManager
{
public:
    // 创建账号管理对象
    explicit AccountManager(std::string filePath);

    // 加载账号文件
    bool load(std::string& error);

    // 注册新账号
    bool registerAccount(const std::string& userId,const std::string& password,std::string& error);

    // 登录已有账号
    bool login(const std::string& userId,const std::string& password,std::string& error);

    // 判断当前是否已经登录
    bool isLoggedIn() const;

    // 获取当前登录用户名
    std::string currentUserId() const;

    // 退出当前账号
    void logout();

private:
    // 保存账号文件路径
    std::string filePath_;

    // 保存已经加载的账号数据
    std::vector<std::pair<std::string, std::string>> accounts_;

    // 保存当前登录用户名
    std::string currentUserId_;

    // 保存账号文件是否加载成功
    bool loaded_ = false;

    // 将当前账号数据保存到文件
    bool save(std::string& error);
};

#endif