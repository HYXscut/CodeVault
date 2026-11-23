# 判题与编译器检查模块课程设计说明（成员 B）

> 对应源码：[compiler_checker.h](../src/compiler/compiler_checker.h)、[compiler_checker.cpp](../src/compiler/compiler_checker.cpp)、[output_judge.h](../src/judge/output_judge.h)、[output_judge.cpp](../src/judge/output_judge.cpp)、[judge_engine.h](../src/judge/judge_engine.h)、[judge_engine.cpp](../src/judge/judge_engine.cpp)。

---

## 一、模块职责与课内知识点映射

成员 B 负责软件的“**编译环境检查**”与“**多测试点判题核心**”，不直接读写题包目录或用户界面控件，全部核心逻辑基于课内 C++ 面向对象与标准库构建，仅将外部进程启动与定时器收口到 Qt 的 `QProcess` / `QTimer` 最小子集中：

| 本模块类 / 设计 | 作用 | 对应课内知识点 |
|---|---|---|
| `CompilerChecker` | 异步执行 `g++.exe --version`，5 秒超时保护，解析真实编译器版本或错误 | 第 7 章类封装（`private` 状态 + `public` 接口）；Qt `QProcess` / `QTimer` 最小异步用法 |
| `OutputJudge` → `TextOutputJudge` → `ExactJudge` | 比较器三层继承体系，在运行时通过基类指针多态比较程序输出与标准答案 | 第 9 章公有继承与 `static` 辅助函数；第 10 章纯虚函数、抽象基类、虚析构函数与运行时多态 |
| `JudgeEngine` | 一次编译、逐点运行、首错即停、超时与输出上限防护、临时目录清理 | 第 6 章 `struct` 数据载体（`Problem`、`TestCase`、`JudgeResult`）；第 11 章 `std::vector` / `std::string` |
| `isToolchainFailure()` | 从编译输出文本识别组件缺失/空输出异常，区分工具链故障与源码错误 | 第 5 章指针数组与数组长度计算；第 11 章 `std::string::find` 与 `string::npos` |

---

## 二、异步编译与多点运行顺序（为什么不用普通 `for` 循环死等）

### 1. 为什么采用“事件驱动异步流程”？
如果在界面线程中写一个 `for` 循环依次调用 `QProcess::waitForFinished()` 阻塞等待编译和 5 个测试点运行，主窗口界面会卡死无响应（尤其学生代码死循环超时时）。因此 `JudgeEngine` 使用 4 个简单整型阶段常量控制状态流转：
- `STAGE_IDLE (0)`：空闲，可接受新任务；
- `STAGE_COMPILING (1)`：正在调用 `g++.exe` 编译；
- `STAGE_RUNNING (2)`：正在逐个运行测试点；
- `STAGE_CLEANING (3)`：正在确认进程退出、删除临时工作目录并结算结果。

### 2. 一次判题的完整执行链

```text
C 调用 startJudge(problem, sourcePath, tests, error)
  ├─ 校验：是否忙碌 / ID / 时限 / 测试点 1..testCount 连续 / 源码与编译器存在
  ├─ 在 <appDir>/work/ 下创建唯一英文临时目录 judge-XXXXXX，检查可写性
  ├─ 将含中文或空格的原源码复制为临时目录下的 main.cpp（保留原 sourcePath 副本）
  └─ 置 stage_ = STAGE_COMPILING，通过 QTimer::singleShot(0) 异步启动 startCompile()

startCompile()  [整次任务只编译一次，绝不复用旧 exe]
  ├─ 设置工作目录为 judge-XXXXXX，并将 <toolchain/bin> 置顶到子进程 PATH
  ├─ 启动编译超时计时器（默认 30000 ms，对应 compileTimeoutMs_）
  └─ 异步执行：g++ -std=c++17 main.cpp -o main.exe

编译器结束 -> onProcessFinished()
  ├─ 若超时 / 输出超限 / 读写错 / 崩溃 / 工具链损坏 / 未生成 main.exe -> 记 InternalError，转 finishJudge()
  ├─ 若正常退出且退出码非 0（经 isToolchainFailure 排除工具链故障）-> 记 CE，不运行测试点，转 finishJudge()
  └─ 若编译成功 -> currentIndex_ = 0，stage_ = STAGE_RUNNING，异步触发 startRun()

startRun()  [逐点运行当前 tests_[currentIndex_]]
  ├─ 重置本点超时/超限/缓冲标志，同样把 <toolchain/bin> 置顶到子进程 PATH（保证无系统 g++ 的电脑能加载运行库 DLL）
  └─ 异步启动 main.exe

被测程序启动成功 -> onProcessStarted()
  ├─ executedCount_ 加 1，启动耗时计时器 runClock_ 与本点超时倒计时（problem_.timeLimitMs）
  └─ 写入 tests_[currentIndex_].input 并立刻调用 closeWriteChannel() 关闭写端（让等待 EOF 的程序正常结束）

当前测试点结束 -> onProcessFinished()
  ├─ 停止定时器，收完残留 stdout / stderr，按优先级判定本点结果
  ├─ 若当前点不是 AC（首错停止）-> 记录 failure testIndex 与输出信息，转 finishJudge()
  ├─ 若当前点 AC 且还有下一点 -> passedCount_ 加 1，currentIndex_ 加 1，异步启动下一点 startRun()
  └─ 若全部测试点 AC -> 记总状态 AC（testIndex = 0），转 finishJudge()

finishJudge()  [统一收尾与单次通知]
  ├─ 确认外部进程已退出（未退出则保持引擎锁定，不发完成信号）
  ├─ 删除临时目录 judge-XXXXXX，置 stage_ = STAGE_IDLE
  └─ 仅发出一次 judgeFinished() 信号通知 C 读取 getResult()
```

---

## 三、时限判断、防重入隔离与输出保护

1. **运行阶段单点时限（`TLE`）**：
   - 每个测试点在 `onProcessStarted()` 成功启动后才启动 `timer_->start(problem_.timeLimitMs)`，编译耗时不计入程序运行耗时。
   - 超时触发 `onTimeout()` 时，先标记 `timedOut_ = true` 再调用 `process_->kill()`；结算时**先判断 `timedOut_` 再看退出码**，防止被系统强杀产生的非零退出码覆盖成 `RE`。
2. **步骤编号与单次结算保护**：
   - 每次进入编译或运行新点时递增 `stepId_`，重置 `stepSettled_ = false`，定时器绑定的 `timerStepId_ = stepId_`。
   - 即使旧点定时器迟到，因 `timerStepId_ != stepId_` 会被 `onTimeout()` 忽略；同一测试点也不会因同时触发 `errorOccurred` 和 `finished` 而被重复结算。
3. **编译超时进程树清理**：
   - 编译器可能派生 `cc1plus.exe`、`as.exe`、`ld.exe` 等子进程。编译超过 `compileTimeoutMs_` 时，在父进程未被杀前读取有效 `PID`，通过 `taskkill /PID <PID> /T /F` 结束整棵进程树（不按程序名误杀无关进程）；若 `taskkill` 自身超时，则显式结束 `killer`、记下错误详情并兜底调用 `process_->kill()`。
4. **`16 MiB` 输出上限保护（`OutputLimit`）**：
   - `collectOutput()` 统计当前点 `stdout` 与 `stderr` 的累计字节数（上限 `16 * 1024 * 1024` 字节）。每次最多读取 `remaining + 1` 字节，一旦超出即置 `outputLimitExceeded_ = true` 并立即 `kill()` 进程，防止死循环打印撑爆内存，也避免拿被截断的输出误判 `AC`/`WA`。

---

## 四、7 种结果状态分类规则

| 状态 | 触发条件 | 对后续点与上层进度的影响 |
|---|---|---|
| `AC` | 全部 `1..testCount` 测试点正常退出且比较器返回 `true` | `testIndex = 0`，学习状态可更新为“本地通过” |
| `WA` | 当前点正常退出，但比较器返回 `false` | 首错即停，学习状态记为“练习中” |
| `CE` | 编译器正常退出、退出码非 `0`，且 `isToolchainFailure()` 返回 `false` | 不运行任何测试点，保留 `compilerOutput` |
| `RE` | 被测程序已成功启动，随后异常崩溃（`CrashExit`）或退出码非 `0`（注：正常退出码 `0` 但有 `stderr` 不判 `RE`） | 首错即停，在 `runtimeError` 中保留 `stderr` 与退出码说明 |
| `TLE` | 当前点运行超过 `problem_.timeLimitMs` 被定时器强杀 | 首错即停，保留时限说明，后续任务可正常继续 |
| `OutputLimit` | 当前点 `stdout + stderr` 累计超过 `16 MiB` | 首错即停，独立上报，不用截断输出判 `AC`/`WA` |
| `InternalError` | 缺编译器、启动失败、编译超时、编译日志超限、编译器崩溃、未生成 `main.exe`、管道读写失败、工具链缺失（如缺 `cc1plus`）、工作目录不可写 | 保留可读 `error`，上层据此**不覆盖原有的有效学习状态** |

---

## 五、输出比较器的继承、多态设计与调用方式

参照课程第 9、10 章的设计模式，输出比较器拆分为三层类结构（见 [output_judge.h](../src/judge/output_judge.h)）：

```text
OutputJudge（抽象基类）
  ├── virtual ~OutputJudge() {}            // 虚析构函数：防止通过基类指针释放子类时泄漏
  └── virtual bool compare(...) const = 0  // 纯虚函数：定义统一比较接口
        ▲
        │ public 继承
TextOutputJudge（文本比较中间基类）
  └── static string normalizeNewlines(...) // 将 Windows "\r\n" 统一为 "\n"，保留空格与末尾换行
        ▲
        │ public 继承
ExactJudge（精确比较派生类）
  └── bool compare(...) const              // 重写纯虚函数：先统一换行符，再用 == 精确比较
```

**在 `JudgeEngine::onProcessFinished()` 中的多态调用**：
```cpp
ExactJudge exactJudge;
OutputJudge *judge = &exactJudge; // 赋值兼容规则：基类指针指向派生类对象
bool correct = judge->compare(pointActual, pointExpected); // 运行时动态联编
```
- **设计优点**：`JudgeEngine` 只依赖抽象基类 `OutputJudge` 的统一接口 `compare()`。未来若要扩展“忽略行末空格的宽松比较器（例如 `IgnoreTrailingSpaceJudge`）”，只需新写一个继承自 `TextOutputJudge` 的子类并重写 `compare()`，把基类指针指向新对象即可，判题主流程代码一行不用改。

---

## 六、课程答辩常见提问速查

1. **为什么 `OutputJudge` 析构函数要加 `virtual`？**  
   第 10 章规则：如果通过基类指针 `OutputJudge*` 去 `delete` 一个动态创建的派生类对象，没有虚析构函数就只会调用基类析构函数而漏掉派生类析构函数，造成内存泄漏；加上 `virtual ~OutputJudge() {}` 才能安全多态析构。
2. **为什么不直接把学生原来的中文路径传给 `g++.exe`，而要复制成 `main.cpp`？**  
   Windows 下 MinGW 工具链对路径中的中文或空格兼容不稳定，且直接在用户源码目录生成 `main.exe` 会弄脏用户的练习文件夹。复制成纯英文临时目录下的 `main.cpp`，既保护用户原目录不被污染、支持中文/空格路径选择，又让编译器稳定运行，同时结果里仍保留用户的原始 `sourcePath`。
3. **如果程序往 `std::cerr` 打了警告信息但返回 `0`，你们会错判成 `RE` 吗？**  
   不会。代码严格按 `exitStatus != QProcess::NormalExit || exitCode != 0` 判断 `RE`，已通过 `StderrWithZeroExit_AC` 边界测试实测验证。
