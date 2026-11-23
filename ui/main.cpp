#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow window;

    if (!window.isStartupReady())
    {
        return 0;
    }

    window.show();
    return app.exec();
}
