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
