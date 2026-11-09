#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QTextEdit>
#include "problem/problem_library.h"
#include "compiler/compiler_checker.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void importPackage();
    void refreshProblems();
    void showSelectedProblems(int row,int column);
    void checkCompiler();
    void onCompilerCheckFinished();

private:
    Ui::MainWindow *ui;
    ProblemLibrary* library_;
    CompilerChecker*checker_;

    QTextEdit* statementText_;
    QPushButton* checkCompilerButton_;
};

#endif // MAINWINDOW_H
