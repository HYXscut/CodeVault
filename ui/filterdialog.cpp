#include "filterdialog.h"
#include "ui_filterdialog.h"

// 构造筛选对话框对象
// 功能说明：初始化对话框界面组件，绑定各操作按钮的点击信号与处理槽函数
// 参数说明：parent 父窗口指针，默认为空
FilterDialog::FilterDialog(QWidget* parent)
    : QDialog(parent),
      ui(new Ui::FilterDialog)
{
    // 根据 UI 描述文件初始化各界面控件
    ui->setupUi(this);

    connect(ui->applyButton, &QPushButton::clicked, this, &FilterDialog::applyFilter);

    connect(ui->clearButton, &QPushButton::clicked, this, &FilterDialog::clearFilter);

    connect(ui->cancelButton, &QPushButton::clicked, this, &FilterDialog::cancelFilter);
}

// 析构筛选对话框对象
// 功能说明：释放 Qt Designer 动态分配的界面对象内存
// 参数说明：无
FilterDialog::~FilterDialog()
{
    // 释放界面 UI 对象
    delete ui;
}

// 获取标题筛选文本
// 功能说明：读取用户在标题输入框中填写的搜索关键字并去除首尾空白字符
// 参数说明：无
// 返回值：去除首尾空格后的标题关键字
// 实现步骤：1. 获取输入框文本 2. 调用 trimmed 去除两端空白并返回
QString FilterDialog::titleText() const
{
    // 读取标题输入框内容并去除首尾空格
    return ui->titleEdit->text().trimmed();
}

// 获取当前选中的标签
// 功能说明：读取标签组合下拉框中当前显示的选中文本
// 参数说明：无
// 返回值：当前选中的标签文本
// 实现步骤：直接读取并返回 tagComboBox 的当前文本
QString FilterDialog::selectedTag() const
{
    // 获取标签下拉框当前选中的文本
    return ui->tagComboBox->currentText();
}

// 获取当前选中的状态
// 功能说明：读取判题状态组合下拉框中当前显示的选中文本
// 参数说明：无
// 返回值：当前选中的状态文本
// 实现步骤：直接读取并返回 statusComboBox 的当前文本
QString FilterDialog::selectedStatus() const
{
    // 获取状态下拉框当前选中的文本
    return ui->statusComboBox->currentText();
}

// 获取当前选中的题包
// 功能说明：读取题包组合下拉框中当前显示的选中文本
// 参数说明：无
// 返回值：当前选中的题包文本
// 实现步骤：直接读取并返回 packageComboBox 的当前文本
QString FilterDialog::selectedPackage() const
{
    // 获取题包下拉框当前选中的文本
    return ui->packageComboBox->currentText();
}

// 确认并应用筛选条件
// 功能说明：确认用户当前的筛选选择，通知主窗口并关闭对话框
// 参数说明：无
// 实现步骤：调用 QDialog 的 accept 方法通知调用方对话框已被确认接受
void FilterDialog::applyFilter()
{
    // 返回 Accepted 状态并关闭对话框，通知主窗口应用筛选结果
    accept();
}

// 清空所有筛选条件
// 功能说明：将对话框中的所有输入控件重置为初始默认状态
// 参数说明：无
// 实现步骤：1. 清空标题编辑框 2. 将标签、状态、题包下拉框重置为第 0 项（全部）
void FilterDialog::clearFilter()
{
    // 清空标题输入框中的文字
    ui->titleEdit->clear();

    // 将标签下拉框重置为默认的第一项（全部标签）
    ui->tagComboBox->setCurrentIndex(0);

    // 将状态下拉框重置为默认的第一项（全部状态）
    ui->statusComboBox->setCurrentIndex(0);

    // 将题包下拉框重置为默认的第一项（全部题包）
    ui->packageComboBox->setCurrentIndex(0);
}

// 取消筛选操作
// 功能说明：放弃本次筛选修改，关闭对话框并返回拒绝状态
// 参数说明：无
// 实现步骤：调用 QDialog 的 reject 方法通知调用方对话框已被取消
void FilterDialog::cancelFilter()
{
    // 返回 Rejected 状态并关闭对话框，通知主窗口放弃筛选修改
    reject();
}

// 设置标签下拉框可选项
// 功能说明：更新标签下拉框列表，自动补全默认“全部标签”选项
// 参数说明：tags 题库中提取的所有有效标签列表
// 实现步骤：1. 清空旧选项 2. 插入默认项“全部标签” 3. 批量添加传入的标签列表
void FilterDialog::setTags(const QStringList& tags)
{
    // 清空当前下拉框中的旧选项
    ui->tagComboBox->clear();

    // 添加默认的全部标签选项作为首选项
    ui->tagComboBox->addItem(QStringLiteral("全部标签"));

    // 批量添加从题库中获取的所有有效标签
    ui->tagComboBox->addItems(tags);
}

// 设置题包下拉框可选项
// 功能说明：更新题包下拉框列表，自动补全默认“全部题包”选项
// 参数说明：packages 题库中提取的所有有效题包列表
// 实现步骤：1. 清空旧选项 2. 插入默认项“全部题包” 3. 批量添加传入的题包列表
void FilterDialog::setPackages(const QStringList& packages)
{
    // 清空当前下拉框中的旧选项
    ui->packageComboBox->clear();

    // 添加默认的全部题包选项作为首选项
    ui->packageComboBox->addItem(QStringLiteral("全部题包"));

    // 批量添加从题库中获取的所有有效题包
    ui->packageComboBox->addItems(packages);
}
