#ifndef CODEVAULT_ACCOUNT_MANAGER_H
#define CODEVAULT_ACCOUNT_MANAGER_H

#include <string>
#include <vector>

/*
 类说明：账号管理类，负责账号的注册、登录验证、文件存储与当前登录会话管理。
 继承与派生：独立的类，无基类继承，无子类派生。
 类的属性说明：
    filePath_: 账号数据文件存储路径（std::string）
    accounts_: 内存中已加载的账号列表（std::vector<std::pair<std::string, std::string>>）
    currentUserId_: 当前已登录用户名（std::string）
    loaded_: 标记账号文件是否已成功加载（bool）
 类的方法说明：
    AccountManager(): 构造函数
    load(): 加载账号文件，支持初次自动创建
    registerAccount(): 注册新账号并查重
    login(): 校验账号密码并登录
    isLoggedIn(): 查询登录状态
    currentUserId(): 获取当前登录用户名
    logout(): 退出登录
    save(): 私有函数，保存账号数据至文件
 */
class AccountManager
{
public:
    // 创建账号管理对象
    // 参数说明：filePath - 账号保存文件路径
    explicit AccountManager(std::string filePath);

    // 加载账号文件
    // 参数说明：error - 传出加载错误提示
    // 返回值：成功返回 true，失败返回 false
    bool load(std::string& error);

    // 注册新账号
    // 参数说明：userId - 用户名；password - 密码；error - 传出错误原因
    // 返回值：注册成功返回 true，失败返回 false
    bool registerAccount(const std::string& userId,const std::string& password,std::string& error);

    // 登录已有账号
    // 参数说明：userId - 用户名；password - 密码；error - 传出错误原因
    // 返回值：登录成功返回 true，失败返回 false
    bool login(const std::string& userId,const std::string& password,std::string& error);

    // 登录并传出用户名
    // 参数说明：userId - 用户名；password - 密码；outUserId - 传出用户名；error - 错误信息
    bool login(const std::string& userId,const std::string& password,std::string& outUserId,std::string& error);

    // 判断当前是否已经登录
    // 返回值：已登录返回 true，未登录返回 false
    bool isLoggedIn() const;

    // 获取当前登录用户名
    // 返回值：返回当前用户名
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
    // 参数说明：error - 传出保存错误原因
    // 返回值：保存成功返回 true，失败返回 false
    bool save(std::string& error);
};

#endif
