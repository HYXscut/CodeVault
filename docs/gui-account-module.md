# 界面与账号模块课程设计说明（成员 C）

> 对应源码：[account_manager.h](../src/account/account_manager.h)、[account_manager.cpp](../src/account/account_manager.cpp)、[mainwindow.h](../ui/mainwindow.h)、[mainwindow.cpp](../ui/mainwindow.cpp)、[mainwindow.ui](../ui/mainwindow.ui)、[logindialog.h](../ui/logindialog.h)、[logindialog.cpp](../ui/logindialog.cpp)、[logindialog.ui](../ui/logindialog.ui)、[filterdialog.h](../ui/filterdialog.h)、[filterdialog.cpp](../ui/filterdialog.cpp)、[filterdialog.ui](../ui/filterdialog.ui)、[main.cpp](../ui/main.cpp)。

---

## 一、模块职责与课内知识点映射

成员 C 负责系统的“**图形用户界面（GUI）调度**”、“**用户账号与多用户会话体系**”、“**多条件组合筛选**”以及“**跨模块业务集成与生命周期管理**”。通过将成员 A 的题库/进度持久化模块与成员 B 的编译器检查/多测试点判题引擎在主窗口中有机组装，向用户提供直观、健壮、美观的操作交互：

| 本模块类 / 设计 | 作用与功能说明 | 对应课内知识点 |
|---|---|---|
| `AccountManager` | 纯 C++ 业务类，负责账号注册、密码验证、查重、会话管理与 `data/accounts.txt` 持久化 | 第 7 章类封装（`private` 数据成员与 `public` 接口）；第 12 章文件流与数据持久化；第 11 章 `std::vector` 与 `std::pair` 容器 |
| `LoginDialog` | 模态对话框，支持登录与注册模式动态切换、二次密码确认与输入有效性校验 | 第 9 章类继承与派生（公有继承 `QDialog`）；事件驱动与信号槽机制 |
| `FilterDialog` | 模态对话框，支持按标题子串、标签、题包 ID 及当前账号做题状态组合筛选 | 第 6 章结构体（构造 `ProblemQuery` 传参）；第 9 章派生类实现 |
| `MainWindow` | 核心界面中枢，调度题库加载、编译器自检、源码绑定、异步判题监控、进度展示与刷新 | 第 9 章公有继承 `QMainWindow`；第 10 章虚函数重写（重写 `closeEvent` 阻断判题中关闭）；对象组合（持有 A、B 模块指针） |
| `setMarkdown()` | 题面区域原生 Markdown 富文本语法解析与排版渲染 | 第 7 章对象方法调用；Qt 6 富文本排版与样式渲染引擎应用 |
| `judgingProgress_` 快照与判题锁 | 异步判题期间深拷贝锁定上下文，界面控件批量互斥禁用 | 状态机思想与并发安全防竞态设计；数据一致性保护 |

---

## 二、核心类体系与架构设计

### 1. 类继承关系与对象生命周期

```text
QObject
 └── QWidget
      ├── QMainWindow
      │    └── MainWindow (主窗口，组合持有 A、B、C 各核心业务对象)
      └── QDialog
           ├── LoginDialog (账号登录与注册模态对话框)
           └── FilterDialog (题目多条件筛选模态对话框)

AccountManager (无基类独立业务类，纯 C++ 面向对象封装，解耦 GUI)
```

- **对象树与内存安全管理**：
  - `MainWindow` 负责创建并持有 `ProblemLibrary`、`ProgressStore`、`JudgeEngine`、`CompilerChecker` 及 `AccountManager` 对象的指针；
  - 析构函数中严格按创建顺序反向释放动态堆资源，避免内存泄露；
  - 子对话框（`LoginDialog`、`FilterDialog`）以 `MainWindow` 作为 `parent` 创建，利用 Qt 内部对象树机制实现自动内存托管。

---

## 三、核心机制与安全设计

### 1. 账号认证与多用户会话隔离

`AccountManager` 管理位于 `data/accounts.txt` 的纯文本账号文件，采用首行版本头 `CODEVAULT_ACCOUNTS_V1` 校验文件格式有效性：
- **注册与查重**：在注册前遍历比对用户名，禁止空用户名、含空格用户名以及重复用户名；
- **文件保存**：采用即时写入与流刷新机制，确保注册成功后即刻落盘；
- **会话隔离**：主窗口通过 `currentUserId_` 记录当前活跃会话身份。所有关于题目的练习进度、最近结果摘要及源码绑定路径均严格绑定到该用户名下。当用户点击“退出登录”时，系统将清空界面展示的私有路径与进度数据，重新弹出 `LoginDialog`，确保甲、乙两用户在同一台设备上的做题记录互不泄露。

### 2. 判题锁与状态互斥防护（Concurrency Guard）

在实际使用场景中，判题过程涉及底层子进程编译与多个测试点的逐一运行（具有耗时性）。为防止用户在此期间进行误操作引发并发混乱，`MainWindow` 实现了严格的**判题锁**机制：
- **提交锁定**：在 `startJudge()` 判定判题请求被引擎成功接受后，界面立即进入“判题中”锁定状态：
  - 禁用“开始判题”、“选择源码”、“清除源码”按钮；
  - 禁用“导入题包”、“移除题包”及“多条件筛选”操作；
  - 禁用题目列表表格切换与账号退出；
- **窗口关闭拦截**：重写 `QMainWindow::closeEvent`，当检测到判题引擎处于活跃状态时，弹出 `QMessageBox::warning` 警告并调用 `event->ignore()` 阻断关闭，杜绝子进程成为孤儿进程或临时文件残留；
- **完成解锁**：在收到 `onJudgeFinished()` 信号后，无条件恢复所有控件可用性，确保界面状态机严丝合缝。

### 3. 判题上下文快照隔离（`judgingProgress_`）

在异步判题调度中，为防止界面发生不可预期的内部状态漂移，`MainWindow` 在向引擎提交任务的瞬间，会深拷贝生成一份只读快照 `judgingProgress_`：
```cpp
judgingProgress_ = currentProgress_; // 锁定当前账号、题包 ID 与题号
```
当 `onJudgeFinished()` 异步触发时，评测结果、执行耗时及错误摘要将精确写回该快照对象中，再交由 `ProgressStore::saveProgress()` 进行原子落盘。这种设计彻底避免了状态竞态问题。

### 4. 题面 Markdown 原生渲染与长输出截断保护

- **Markdown 题面渲染**：题包中的 `statement.md` 文件支持 Markdown 语法（标题、列表、粗体、代码块等）。主窗口采用 `ui->statementText->setMarkdown(QString::fromUtf8(problem.statement.c_str()))`，利用 Qt 6 底层 Markdown 引擎进行渲染，相比普通纯文本具有更优的教学阅读体验；
- **长输出保护机制**：面对学生代码恶意死循环打印巨量字符或超大输出时，`MainWindow` 在 `actualOutputText` 中截取前 4096 字节并追加 `[输出过长，界面仅显示前 4096 字节]` 友好提示，避免 QTextEdit 因超大富文本排版陷入假死，保障 GUI 流畅响应。

### 5. 源码失效检测与重新绑定

当用户选定题目后，主窗口调用 `QFileInfo(sourcePath_).isFile()` 进行实时探测：
- 若本地源码文件被移动、重命名或删除，界面会在判题按钮旁明确显示红字“源码路径失效”，并在状态栏给出重新选择指引；
- 此时判题按钮处于禁用保护状态，直到用户重新选择有效源文件，增强了系统的容错鲁棒性。

---

## 四、跨模块协作流程图

```text
[用户交互] ──> MainWindow
                 │
                 ├── 启动阶段: AccountManager::load() ──> LoginDialog 认证
                 │
                 ├── 数据加载: ProblemLibrary::load() ──> 刷新展示题目列表表格
                 │              ProgressStore::load() ──> 恢复当前用户各题练习记录
                 │
                 ├── 源码选择: QFileDialog::getOpenFileName() ──> 校验存在性并绑定
                 │
                 ├── 组合筛选: FilterDialog ──> 构造 ProblemQuery ──> ProblemLibrary::searchProblems()
                 │
                 ├── 评测阶段: JudgeEngine::startJudge() ──> 激活判题锁
                 │                │
                 │                └── [后台异步编译与运行] ──> emit judgeFinished()
                 │
                 └── 完成回调: onJudgeFinished() ──> 展示逐点详情与比对
                                └── ProgressStore::saveProgress() ──> 原子更新落盘
```

---

## 五、总结

界面与账号模块在设计上坚持“**高内聚、低耦合、强容错**”的原则，所有业务数据流转清晰，错误提示友好详尽。经本机回归与 50 题筛选压测验证，模块在多账号隔离、异常路径恢复、长文本保护及并发互斥等方面均完全符合第五周终期交付标准。
