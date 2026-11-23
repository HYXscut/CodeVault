# 第三周成员 B：独立判题引擎测试记录

日期：2026-11-21。

范围：在尚未合入成员 A 第三周新版 `types.h`（多点详细字段 `pointResults` / 三个计数）的前提下，独立编译并运行升级后的 `JudgeEngine` 与 `JudgeEngineTest.exe`。使用 5 个测试点（输入 `1 2`、`10 20`、`100 200`、`7 8`、`0 0`，时限 `1000 ms`）对多点接力、首错停止、错误分类及连续复用稳定性进行验证。

## 本次已验证

命令：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Test -Case ALL
```

工具链：Qt 6.12.0、MinGW 15.1、CMake 3.30.5、Ninja 1.12.1；构建目录为 `D:/CodeJudgeBuild`；`JudgeEngineTest.exe` 与 `CodeVaultUI.exe` 均编译链接通过。

| 验证项目 | 用例数 | 预期行为 | 实际结果 | 结论 |
|---|---|---|---|---|
| 5 点全部通过（`AC`） | 5 次（第 1、7、9、14、20 轮） | `status == AC`，`testIndex == 0`，只编译一次，末点输出 `0\n` | 均返回 `AC`，`Failed point: 0`，总耗时 `146~194 ms` | 通过 |
| 第 3 点答案错误（`WA3`） | 3 次（第 2、10、17 轮） | 在第 3 点停止，`status == WA`，`testIndex == 3`，输出 `301\n` | 均返回 `WA`，`Failed point: 3`，总耗时 `96~102 ms` | 通过 |
| 首点答案错误（`WA1`） | 2 次（第 3、16 轮） | 在第 1 点停止，`status == WA`，`testIndex == 1`，输出 `4\n` | 均返回 `WA`，`Failed point: 1`，耗时 `35 ms` | 通过 |
| 源码编译错误（`CE`） | 3 次（第 4、12、19 轮） | `status == CE`，`testIndex == 0`，保留编译日志，不运行测试点 | 均返回 `CE`，`Failed point: 0`，运行耗时 `0 ms` | 通过 |
| 非零退出运行错误（`RE`） | 3 次（第 5、11、18 轮） | `status == RE`，`testIndex == 1`，保留 `stderr` 与退出码 `7` | 均返回 `RE`，`Failed point: 1`，耗时 `36~37 ms` | 通过 |
| 死循环超时（`TLE`） | 2 次（第 6、13 轮） | `status == TLE`，`testIndex == 1`，耗时约 `1000 ms`，后续任务可恢复 | 均返回 `TLE`，`Failed point: 1`，耗时 `999~1001 ms`，紧接下一轮 `AC` 通过 | 通过 |
| 输出超限（`OutputLimit`） | 2 次（第 8、15 轮） | 超过 `16 MiB` 提前终止，独立状态 `OutputLimit`，`testIndex == 1` | 均返回 `OutputLimit`，`Failed point: 1`，耗时 `56~62 ms`，紧接下一轮 `AC` 通过 | 通过 |
| 缺失编译器拒绝（`MissingCompiler`） | 1 次 | `startJudge` 返回 `false`，填写可读原因，`isBusy() == false` | `Accepted: false`，`Error: 找不到g++编译器` | 通过 |

说明：以上 20 次混合任务全部复用同一个 `JudgeEngine` 实例，每次均仅收到 `1` 次 `judgeFinished()` 完成通知且读取结果时 `isBusy()` 已恢复为 `false`，`JudgeEngineTest.exe` 退出码为 `0`（`Passed 20/20 multi-point cases.`）。

## 后续待完成与三人联调项

1. **待成员 A 提交新版 `src/common/types.h` 后**：在 `JudgeEngine` 中补充将每点结果填入 `result_.pointResults` 以及填写 `passedCount` / `executedCount` / `totalCount` 的最后收尾代码，并补充针对逐点表和三个计数的断言。
2. **与成员 A 的对接验证**：接入 `samples/week3_demo` 5 点题包及 `ProblemLibrary::getTestCase(..., 1..testCount, ...)` 真实文件读取。
3. **与成员 C 的共同验收**：在主窗口中完成多点判题显示、`ProgressStore` 持久化保存、重启恢复、原源码失效提示、文件损坏与保存失败处理等全部第三周验收清单。
4. **极端环境补充测试**：按需补充编译器启动失败/编译超时（`taskkill /PID ... /T /F`）的专项模拟记录。

## 成员 A：week3_demo 真实题包联调预期

题包：samples/week3_demo
题包 ID：week3_demo
题目 ID：P001
测试点数量：5
每点时限：1000 ms

已完成独立检查：
- 首次导入成功。
- 再次运行时从保存副本恢复成功。
- 题面、时限和测试点数量检查通过。
- 第 1～5 点输入与标准输出的完整内容检查通过。

以下为正式判题预期；对应手工验收结果见后面的“成员 A：真实 GUI 联调结果”。

| 源码（位于 examples） | 总状态 | 第1点 | 第2点 | 第3点 | 第4点 | 第5点 | passedCount | executedCount | totalCount | testIndex |
|---|---|---|---|---|---|---|---:|---:|---:|---:|
| answer_multi_ac.cpp | AC | AC | AC | AC | AC | AC | 5 | 5 | 5 | 0 |
| answer_wa_at3.cpp | WA | AC | AC | WA | NotRun | NotRun | 2 | 3 | 5 | 3 |
| answer_wa_at1.cpp | WA | WA | NotRun | NotRun | NotRun | NotRun | 0 | 1 | 5 | 1 |
| answer_re.cpp | RE | RE | NotRun | NotRun | NotRun | NotRun | 0 | 1 | 5 | 1 |
| answer_tle.cpp | TLE | TLE | NotRun | NotRun | NotRun | NotRun | 0 | 1 | 5 | 1 |
| answer_output_limit.cpp | OutputLimit | OutputLimit | NotRun | NotRun | NotRun | NotRun | 0 | 1 | 5 | 1 |
| answer_ce.cpp | CE | NotRun | NotRun | NotRun | NotRun | NotRun | 0 | 0 | 5 | 0 |

补充检查：
- answer_wa_at3.cpp 在第三点输入 0 7 时输出 8，标准输出为 7。
- RE 应保留非零退出码 7 和 stderr 信息。
- CE 应保留编译日志，不启动被测程序。
- 输出超限应独立显示 OutputLimit。
- TLE 和输出超限任务结束后，立即运行 AC 样例，确认引擎可以继续使用。
- 每次任务只发出一次完成通知。

## 成员 A：真实 GUI 联调结果

以下项目已在 week3_demo/P001 上通过手工检查：

- 全部 AC：计数 5/5/5，五点均为 AC。
- 第三点 WA：计数 2/3/5，后两点为 NotRun。
- 首点 WA、RE、TLE、CE 的状态与计数符合预期。
- 输出超限显示 OutputLimit；限制实际输出预览后界面可正常响应。
- TLE、输出超限后再次执行 AC，任务正常完成。
- AC 后重启恢复源码路径与“本地通过”状态。
- WA 后重启恢复源码路径与“练习中”状态。
- 清除源码后重启仍无绑定，开始判题不可用。
- 中文和空格路径可判题，重启后可恢复。
- 源码文件改名后显示路径失效，禁止判题；恢复文件名后可重新判题。

后续手工验收结果：

- 缺少 g++.exe 时拒绝判题请求，显示具体原因，不覆盖旧进度；恢复后可 AC。
- 缺少 cc1plus.exe 时返回 InternalError，五点均为 NotRun，计数 0/0/5；进度文件经 fc /b 比较无差异，恢复工具后可 AC。
- 进度文件标识损坏时明确报错并停止进入主界面；恢复备份后正常。
- 启动前独占锁定进度文件时明确报错；解锁后正常恢复。
- 已加载后锁定进度文件，WA 判题结果仍可见，并明确提示保存失败。
- 保存失败后，旧文件经 fc /b 比较无差异；刷新及重启后旧状态保留。
- 解锁后再次判题，进度保存恢复正常。
- 当前整合版本在同一进程中连续完成 20 次混合判题，状态、计数及逐点结果符合预期，界面可继续操作。
- 第 20 次 AC 后重启，源码路径与本地通过状态正常恢复。

验证范围说明：
- 以上为当前整合版本的手工 GUI 验收。
- 未对 judgeFinished 信号次数做自动断言，不据此声称其精确次数已验证。
- 成员 B 前面的独立引擎测试记录保留，使用的数据与本节真实题包不同。