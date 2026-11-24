#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    // 初始化 Qt 应用程序对象
    QApplication app(argc, argv);

    // 创建主窗口对象
    MainWindow window;

    // 判断启动与登录流程是否就绪，若未就绪则直接退出程序
    if (!window.isStartupReady())
    {
        return 0;
    }

    // 显示主窗口界面
    window.show();

    // 进入 Qt 主事件循环，等待用户操作与事件响应
    return app.exec();
}
