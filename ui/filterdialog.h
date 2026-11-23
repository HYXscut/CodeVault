#ifndef FILTERDIALOG_H
#define FILTERDIALOG_H

#include <QDialog>
#include <QString>
#include <QStringList>

namespace Ui {
class FilterDialog;
}

class FilterDialog : public QDialog
{
    Q_OBJECT

public:
    // 创建筛选对话框
    explicit FilterDialog(QWidget* parent = nullptr);
    // 释放界面对象
    ~FilterDialog() override;

    // 返回标题输入框中的筛选文字
    QString titleText() const;
    // 返回当前选择的标签
    QString selectedTag() const;
    // 返回当前选择的状态
    QString selectedStatus() const;
    // 返回当前选择的题包
    QString selectedPackage() const;
    // 设置标签下拉框内容
    void setTags( const QStringList& tags);
    // 设置题包下拉框内容
    void setPackages(const QStringList& packages);

private slots:
    // 用户点击“确定”按钮
    void applyFilter();
    // 用户点击“清空”按钮
    void clearFilter();
    // 用户点击“取消”按钮
    void cancelFilter();

private:
    Ui::FilterDialog* ui; // Qt Designer 生成的界面对象
};

#endif
