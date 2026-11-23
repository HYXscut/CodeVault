#include "filterdialog.h"
#include "ui_filterdialog.h"

FilterDialog::FilterDialog(QWidget* parent)
    : QDialog(parent),
      ui(new Ui::FilterDialog)
{
    ui->setupUi(this); // 根据 filterdialog.ui 创建界面控件

    // 将确定按钮连接到筛选确认槽函数
    connect(ui->applyButton,
            &QPushButton::clicked,
            this,
            &FilterDialog::applyFilter);

    connect(ui->clearButton,
            &QPushButton::clicked,
            this,
            &FilterDialog::clearFilter);

    connect(ui->cancelButton,
            &QPushButton::clicked,
            this,
            &FilterDialog::cancelFilter);
}

FilterDialog::~FilterDialog()
{
    delete ui; // 释放 Qt Designer 创建的界面对象
}

QString FilterDialog::titleText() const
{
    return ui->titleEdit->text().trimmed(); // 读取标题并去除首尾空格
}

QString FilterDialog::selectedTag() const
{
    return ui->tagComboBox->currentText(); // 读取当前标签选项
}

QString FilterDialog::selectedStatus() const
{
    return ui->statusComboBox->currentText(); // 读取当前状态选项
}

QString FilterDialog::selectedPackage() const
{
    return ui->packageComboBox->currentText(); // 读取当前题包选项
}

void FilterDialog::applyFilter()
{
    accept(); // 返回 Accepted，通知主窗口确认筛选
}

void FilterDialog::clearFilter()
{
    ui->titleEdit->clear(); // 清空标题输入框
    ui->tagComboBox->setCurrentIndex(0); // 恢复为全部标签
    ui->statusComboBox->setCurrentIndex(0); // 恢复为全部状态
    ui->packageComboBox->setCurrentIndex(0); // 恢复为全部题包
}

void FilterDialog::cancelFilter()
{
    reject(); // 返回 Rejected，通知主窗口取消筛选
}

// 设置标签下拉框内容
void FilterDialog::setTags(
    const QStringList& tags)
{
    // 清空旧的标签选项
    ui->tagComboBox->clear();

    // 添加默认的全部标签选项
    ui->tagComboBox->addItem(QStringLiteral("全部标签"));

    // 添加题库中的标签
    ui->tagComboBox->addItems(tags);
}

// 设置题包下拉框内容
void FilterDialog::setPackages(const QStringList& packages)
{
    // 清空旧的题包选项
    ui->packageComboBox->clear();

    // 添加默认的全部题包选项
    ui->packageComboBox->addItem(QStringLiteral("全部题包"));

    // 添加题库中的题包
    ui->packageComboBox->addItems(packages);
}