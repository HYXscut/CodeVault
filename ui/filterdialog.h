#ifndef FILTERDIALOG_H
#define FILTERDIALOG_H

#include <QDialog>
#include <QString>
#include <QStringList>

namespace Ui {
class FilterDialog;
}

/*
 类说明：题目筛选对话框类，提供按题目标题关键字、标签、判题状态和所属题包进行多条件组合筛选的交互界面。
 继承与派生：公有继承自 QDialog
 类的属性说明：
    ui: Qt Designer 生成的界面控件管理对象指针（Ui::FilterDialog*）
 类的方法说明：
    FilterDialog(): 构造函数，初始化筛选对话框界面与信号槽连接
    ~FilterDialog(): 析构函数，释放界面资源
    titleText(): 获取标题输入框中输入的筛选关键字
    selectedTag(): 获取用户当前选中的标签筛选条件
    selectedStatus(): 获取用户当前选中的判题状态筛选条件
    selectedPackage(): 获取用户当前选中的题包筛选条件
    setTags(): 动态设置并填充标签下拉框的可选项列表
    setPackages(): 动态设置并填充题包下拉框的可选项列表
    applyFilter(): 确认筛选并关闭对话框（私有槽函数）
    clearFilter(): 清空输入并重置所有筛选下拉框为默认项（私有槽函数）
    cancelFilter(): 取消筛选并关闭对话框（私有槽函数）
 */
class FilterDialog : public QDialog
{
    Q_OBJECT

public:
    // 创建筛选对话框对象
    // 参数说明：parent - 父窗口指针，默认为 nullptr
    explicit FilterDialog(QWidget* parent = nullptr);

    // 释放界面对象并析构
    ~FilterDialog() override;

    // 获取标题输入框中的筛选文字
    // 返回值：返回去除首尾空格后的标题关键字字符串
    QString titleText() const;

    // 获取当前选择的题目标签
    // 返回值：返回标签下拉框当前选中的文本
    QString selectedTag() const;

    // 获取当前选择的判题状态
    // 返回值：返回状态下拉框当前选中的文本
    QString selectedStatus() const;

    // 获取当前选择的所属题包
    // 返回值：返回题包下拉框当前选中的文本
    QString selectedPackage() const;

    // 设置并填充标签下拉框可选项
    // 参数说明：tags - 外部题库提取出的有效标签列表
    void setTags(const QStringList& tags);

    // 设置并填充题包下拉框可选项
    // 参数说明：packages - 外部题库提取出的有效题包列表
    void setPackages(const QStringList& packages);

private slots:
    // 确认并应用筛选条件
    // 功能说明：响应确定按钮点击，接受筛选并关闭对话框
    void applyFilter();

    // 清空所有筛选条件
    // 功能说明：响应清空按钮点击，重置所有筛选输入项至默认状态
    void clearFilter();

    // 取消筛选操作
    // 功能说明：响应取消按钮点击，放弃筛选并退出对话框
    void cancelFilter();

private:
    // Qt Designer 生成的界面管理对象指针
    Ui::FilterDialog* ui;
};

#endif // FILTERDIALOG_H
