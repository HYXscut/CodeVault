# CodeJudge Local 第三周公共接口与语义约定（成员 B 统筹）

> 适用成员：A（题库与进度）、B（判题引擎与统筹）、C（主界面与联调）  
> 本文根据《CodeVault_五周计划第三周》整理，供三人确认后共同遵守；具体模块内部实现由各自负责人决定。

---

## 一、公共数据结构约定（待由成员 A 汇总更新至 `src/common/types.h`）

第一、二周的 `Problem`、`CompilerInfo`、`TestCase` 字段保持不变（`TestCase.index` 扩展为 `1..testCount`）。第三周在 `src/common/types.h` 中新增 `TestPointResult`、`Progress` 与学习状态常量，并替换原有 `JudgeResult`：

- **`TestPointResult`**：记录单个测试点结果（`index`、`executed`、`status`、`elapsedMs`、`actualOutput`、`expectedOutput`、`runtimeError`、`error`），未执行点默认 `status = "NotRun"`、`executed = false`、`elapsedMs = 0`。
- **扩展 `JudgeResult`**：在第二周字段基础上增加 `passedCount`、`executedCount`、`totalCount` 和 `std::vector<TestPointResult> pointResults`。
- **过渡字段约定**：
  - `actualOutput` / `expectedOutput` / `runtimeError`：有失败或出错点时填该点内容；全部 `AC` 时填最后一个点内容；未运行任何点（如 `CE`、编译阶段 `InternalError`）时为空。
  - `testIndex`：首个失败或出错点编号；全部 `AC` 或 `CE` 时为 `0`。
  - `elapsedMs`：所有已执行测试点的运行耗时之和（不含编译耗时）。
- **`Progress` 与状态常量**：`ProgressNotStarted = 0`、`ProgressInProgress = 1`、`ProgressLocalAccepted = 2`，通过 `userId`（本周固定 `"practice"`）+ `packageId` + `problemId` 三个字段分别比较定位记录。

> **当前仓库同步状态说明**：成员 B 已在 `JudgeEngine` 内部完成多点循环、错误分类与防护逻辑；待成员 A 将上述结构体更新至 `src/common/types.h` 后，成员 B 只需在 `JudgeEngine` 结算处补充向 `passedCount`、`executedCount`、`totalCount`、`pointResults` 写入数据的收尾代码即可与 C 联调。

---

## 二、7 种状态语义与错误边界

第三周严格区分**学生代码自身问题**与**工具链/系统异常**（界面侧在遇到 `InternalError` 时不得覆盖用户原有的有效学习状态）：

| 状态 | 触发条件 | 对后续点与学习状态的影响 |
|---|---|---|
| `AC` | 全部 `1..testCount` 测试点均正常退出且输出匹配 | `testIndex = 0`，学习状态可更新为本地通过（`2`） |
| `WA` | 当前点正常退出，统一换行后精确比较不一致 | 停在当前点，后续点保持 `NotRun`，学习状态为练习中（`1`） |
| `CE` | 编译器**正常退出**（`NormalExit`）且退出码非 `0` | 不运行测试点，保留 `compilerOutput`，学习状态为练习中（`1`） |
| `RE` | 被测程序已成功启动，之后崩溃（`CrashExit`）或非零退出 | 停在当前点，在 `runtimeError` 中保留 stderr 与退出码说明 |
| `TLE` | 当前点运行超出 `problem.timeLimitMs`，由判题器强制终止 | 停在当前点，不被后续强杀的退出码覆盖成 `RE` |
| `OutputLimit` | 当前点 stdout 与 stderr 累计超过 `16 MiB` 上限 | 停在当前点，独立上报，不用截断输出判 `AC`/`WA` |
| `InternalError` | 缺编译器、编译器/被测程序启动失败、编译超时（`>30s`）、编译日志超限、编译器崩溃、未生成 `main.exe`、管道读写失败、收尾清理异常 | 保留可读 `error`，**不覆盖旧学习状态** |
| `NotRun` | 逐点结果列表中的后续未执行点 | 仅用于 `TestPointResult.status`，不作为整次任务总状态 |

---

## 三、三个计数与首错停止口径

1. `totalCount` 等于 `problem.testCount`（必须为 `1..20`，`0` 个点由 `startJudge` 直接拒绝，不能直接判 `AC`）。
2. `executedCount` 只统计**确实成功启动过被测程序**的点数；编译失败时为 `0`，某点启动失败（`FailedToStart`）时该点 `executed = false` 不计入。
3. `passedCount` 只统计 `AC` 点，恒满足 `0 <= passedCount <= executedCount <= totalCount`。
4. **首错即停示例**：5 点题目在第 3 点 `WA` 时，显示“通过 `2`，已执行 `3`，总点数 `5`”，`testIndex = 3`，第 4、5 点保持 `NotRun`。

---

## 四、`JudgeEngine` 调用与已知清理限制

- **公开接口不变**：保留第二周 `startJudge(problem, sourcePath, tests, error)`、`getResult()`、`isBusy()` 和信号 `judgeFinished()`。C 需先通过 `ProblemLibrary::getTestCase` 循环读取全部 `1..testCount` 测试点，全部成功后再调用 `startJudge`。
- **只编译一次**：每次任务新建唯一英文临时目录，编译生成 `main.exe` 后按编号依次启动新进程运行各测试点，绝不复用旧任务可执行文件。
- **旧事件与重入隔离**：内部维护任务/步骤编号与单次结算标记，同一测试点不会因 `errorOccurred` 和 `finished` 重复结算，旧点迟到的定时器不会终止新点。
- **编译超时进程树清理限制**：编译超过 30 秒时，在父进程退出前读取有效 `PID` 并调用 `taskkill /PID <PID> /T /F` 结束编译器及其子进程；不按进程名称批量查杀。
- **收尾锁定约定**：若外部进程经终止后仍未退出（`state() != NotRunning`），`JudgeEngine` 保持 `isBusy() == true` 不发 `judgeFinished()`，防止新老任务并发冲突；若进程已确认退出但临时目录删除失败，本次结果记为 `InternalError` 提示告警，下一次任务仍使用全新目录。
