#include "account/account_manager.h"
#include <filesystem> // 创建账号文件所在目录
#include <fstream>
#include <utility>// 使用 std::move

// 构造账号管理对象
AccountManager::AccountManager(std::string filePath): filePath_(std::move(filePath))
{
}

// 加载账号文件
bool AccountManager::load(std::string& error)
{
    // 清空当前已经加载的账号
    accounts_.clear();

    // 打开账号文件
    std::ifstream input(filePath_);

    // 判断账号文件是否打开失败
    if (!input)
    {
        // 创建账号文件所在目录
        const std::filesystem::path filePath(filePath_);
        std::error_code directoryError;
        std::filesystem::create_directories(filePath.parent_path(), directoryError);

        // 判断账号目录创建是否失败
        if (directoryError)
        {
            error = "无法创建账号文件目录";
            loaded_ = false;
            return false;
        }

        // 尝试创建空的账号文件
        std::ofstream createFile(filePath_);

        // 判断账号文件是否创建失败
        if (!createFile)
        {
            error = "无法创建账号文件";

            loaded_ = false;

            return false;
        }

        // 标记账号文件加载成功
        loaded_ = true;

        // 清空旧错误
        error.clear();

        return true;
    }

    // 保存当前读取到的用户名
    std::string userId;

    // 保存当前读取到的密码
    std::string password;

    // 循环读取账号文件中的每一行账号
    while (input >> userId >> password)
    {
        // 将当前账号加入内存列表
        accounts_.push_back({userId, password});
    }

    // 判断文件读取过程中是否发生底层错误
    if (input.bad())
    {
        // 写入读取失败原因
        error = "读取账号文件失败";

        // 标记账号文件加载失败
        loaded_ = false;

        // 返回加载失败
        return false;
    }

    // 标记账号文件加载成功
    loaded_ = true;

    // 清空旧错误
    error.clear();

    // 返回加载成功
    return true;
}

// 注册新账号
bool AccountManager::registerAccount(const std::string& userId,const std::string& password,std::string& error)
{
    // 判断账号文件是否已经加载
    if (!loaded_)
    {
        error = "账号文件尚未加载";
        return false;
    }

    // 判断用户名是否为空
    if (userId.empty())
    {
        error = "用户名不能为空";
        return false;
    }

    // 判断密码是否为空
    if (password.empty())
    {
        error = "密码不能为空";

        return false;
    }

    // 遍历已经存在的账号
    for (const auto& account : accounts_)
    {
        // 判断用户名是否重复
        if (account.first == userId)
        {
            error = "用户名已经存在";

            return false;
        }
    }

    // 将新账号加入内存列表
    accounts_.push_back({userId, password});

    // 尝试保存新的账号列表
    if (!save(error))
    {
        // 删除刚刚加入的账号
        accounts_.pop_back();

        return false;
    }

    // 清空旧错误
    error.clear();

    // 返回注册成功
    return true;
}

// 登录账号
bool AccountManager::login(const std::string& userId,const std::string& password,std::string& error)
{
    // 判断账号文件是否已经加载
    if (!loaded_)
    {
        error = "账号文件尚未加载";

        return false;
    }

    // 遍历内存中的账号列表
    for (const auto& account : accounts_)
    {
        // 判断用户名和密码是否同时匹配
        if (account.first == userId && account.second == password)
        {
            // 保存当前登录用户名
            currentUserId_ = userId;

            // 清空旧错误
            error.clear();

            // 返回登录成功
            return true;
        }
    }

    // 写入账号或密码错误提示
    error = "用户名或密码错误";

    return false;
}

// 判断当前是否已经登录
bool AccountManager::isLoggedIn() const
{
    // 根据当前用户名是否为空判断登录状态
    return !currentUserId_.empty();
}

// 获取当前登录用户名
std::string AccountManager::currentUserId() const
{
    // 返回当前登录用户名
    return currentUserId_;
}

// 退出当前账号
void AccountManager::logout()
{
    // 清空当前登录用户名
    currentUserId_.clear();
}

// 保存账号列表
bool AccountManager::save(std::string& error)
{
    // 打开账号文件进行覆盖写入
    std::ofstream output(filePath_);

    // 判断账号文件是否打开失败
    if (!output)
    {
        // 写入账号文件打开失败原因
        error = "无法写入账号文件";

        // 返回保存失败
        return false;
    }

    // 遍历内存中的全部账号
    for (const auto& account : accounts_)
    {
        // 将用户名和密码写入文件
        output << account.first<< ' '<< account.second<< '\n';
    }

    // 判断写入过程中是否发生错误
    if (!output)
    {
        // 写入保存失败原因
        error = "保存账号文件失败";

        // 返回保存失败
        return false;
    }

    // 清空旧错误
    error.clear();

    // 返回保存成功
    return true;
}
