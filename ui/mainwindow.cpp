#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->statusLabel->setText(QStringLiteral("Qt 界面工程已加载，等待连接业务功能"));
}

MainWindow::~MainWindow()
{
    delete ui;
}
