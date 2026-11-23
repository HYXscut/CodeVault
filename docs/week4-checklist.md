# 第四周成员 B：判题回归、工具链验证与统筹验收记录

> 登记范围：成员 B 负责的 `CompilerChecker`、`OutputJudge`、`JudgeEngine` 及独立测试入口；A/C 第三、四周源码尚未合入本仓库，合入后需补充联调与主窗口试打包记录。

---

## 一、可复跑测试入口（供本机与队友复现）

测试工程位于 `tests/CMakeLists.txt`，由 `build.ps1` 从纯英文构建目录配置运行（避免中文工程路径导致 MinGW 汇编器写 `.obj` 失败）：

```powershell
# 1. 全量专项回归（20 轮多点 + 缺编译器 + 残缺工具链 + 6 项边界 + 内置工具链 + 编译超时进程树）
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Test -Case ALL

# 2. 第 4 周 50 次混合判题压力回归（含 work/ 零残留统计）
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Test -Case MIX50

# 3. 单项专项测试（按需替换：BOUNDARY / CTREE / IE / TOOLCHAIN）
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Test -Case BOUNDARY
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Test -Case CTREE
```

---

## 二、已完成独立验证汇总

### 1. 全量专项回归（`build.ps1 -Test -Case ALL`，退出码 `0`）

| 验证模块 | 用例数 | 验证重点 | 实测结果 | 结论 |
|---|---|---|---|---|
| 20 轮多点基础回归 | 20 轮 | `AC`/`WA3`/`WA1`/`CE`/`RE`/`TLE`/`OutputLimit` 交替切换，含异常后立即 `AC`、通知仅 1 次、恢复空闲、`work/judge-*` 零残留 | `20/20` 通过，通知 `20`，`Max residual dirs: 0` | 通过 |
| 缺失编译器拒绝 | 1 次 | 传入不存在的 `g++.exe` 时 `startJudge` 返回 `false`，填写错误原因且保持空闲 | `Accepted: false`，提示找不到编译器 | 通过 |
| 残缺工具链分类 | 1 次 | 仅复制 `g++.exe`（缺少 `cc1plus`）测试 `AC` 源码，验证不再误判为源码 `CE` | 捕获 `cannot execute 'cc1plus'`，正确判为 `InternalError` | 通过 |
| 输入输出边界矩阵 | 6 项 | `EmptyIO_AC`、空输入遇 EOF 不阻塞 `EmptyInput_NonEmptyOut_AC`、空输出错判 `EmptyOut_Mismatch_WA`、多行 CRLF/LF 统一 `Multiline_CRLF_AC`、末尾少换行拒过 `TrailingNewline_WA`、正常退出带 `stderr` 不误判 RE `StderrWithZeroExit_AC` | `6/6` 全部 `PASS` | 通过 |
| 工具链版本与基本判题 | 3 轮 | 验证 `CompilerChecker` 成功读取 `g++ 15.1.0` 版本并完成 `AC → CE → AC` | `Compiler found: true`，`3/3` 通过，`residual=0` | 通过 |
| 编译超时进程树收尾 | 1 次 | 伪编译器通过 `system()` 拉起 `cj_tree_child.exe`（输出 `CHILD_ALIVE` 确认已启动），`setCompileTimeoutMs(1200)` 触发超时后检查收尾 | 返回 `InternalError`（编译超时）；受当前 AI 沙盒权限限制，`taskkill /T` 超时并由兜底 `killer.kill()` + `process_->kill()` 安全接管，未发生卡死或崩溃（普通桌面 PowerShell 建议再复跑一次 `CTREE` 核对干净退出） | 通过（沙盒受限兜底通过） |

### 2. 第 4 周 50 次混合判题压力回归（`build.ps1 -Test -Case MIX50`，退出码 `0`）

- **测试序列**：以 `AC → WA3 → CE → RE → TLE → AC → OL → WA1 → WA3 → AC` 为 1 组，同一 `JudgeEngine` 实例连续执行 5 组共 **50 次**任务。
- **实测汇总输出**：
  - `Total passed: 50/50`
  - `Counts by status: AC=15, WA=15, CE=5, RE=5, TLE=5, OutputLimit=5`
  - `Total notifications: 50 (expected 50)`（每轮严格 1 次 `judgeFinished`）
  - `Max residual dirs: 0`（每轮结束后 `work/judge-*` 目录残留数全部为 `0`）
  - `Final engine busy: false`（每轮及结束后均立即可发起下一轮任务）

### 3. 脱离开发环境路径的便携试跑验证（模拟无系统 `g++` 电脑）

- **试包目录**：临时英文独立目录 `portable_demo/`，包含 `JudgeEngineTest.exe`、`Qt6Core.dll`、运行库 DLL 以及由 `D:/Qt/Qt/Tools/mingw1510_64` 复制来的完整 `toolchain/`。
- **隔离环境**：命令行仅保留系统路径 `PATH = C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem`（完全剔除 `D:\Qt` 与系统 MinGW 路径）。
- **执行命令**：`JudgeEngineTest.exe --bundled TOOLCHAIN`（从 `.exe` 所在目录自动拼出 `toolchain/bin/g++.exe`）。
- **验证结果**（退出码 `0`）：
  - `CompilerChecker` 正常启动并读出 `g++.exe (x86_64-posix-seh-rev0, Built by MinGW-Builds project) 15.1.0`；
  - `JudgeEngine` 通过 `startCompile()` 与修复后的 `startRun()`（向运行阶段 `PATH` 置顶 `toolchain/bin`）跑通 `AC → CE → AC`（`3/3` 通过，`Max residual dirs: 0`）。

---

## 三、待 A / C 代码合入后的联合验收清单

当前仓库尚待合入 A 的第三、四周提交及 C 的账号/部署部分，合入后按以下顺序完成最终验收：

- [ ] **公共字段补齐与联调**：在 A 合入包含 `TestPointResult`、`Progress` 与扩展 `JudgeResult` 的 `src/common/types.h` 后，在 `JudgeEngine` 结算处补写 `passedCount`、`executedCount`、`totalCount` 与 `pointResults`，并用 `samples/week3_demo` 5 点题包完成全 `AC` 与第 3 点 `WA` 验证。
- [ ] **双账号同题与会话隔离**：验证甲、乙账号在同一共享题库下分别绑定不同源码并各自保存独立进度与状态；验证判题过程中无法退出或切换账号，结果写回发起任务时的账号；`practice` 原记录保持独立。
- [ ] **50 道题筛选与题包移除/恢复**：配合 A/C 验收标题、单标签、状态、题包及组合筛选，以及移除题包重启仍隐藏、同内容重导入恢复、同 ID 改内容拒绝。
- [ ] **普通桌面环境复核 `CTREE`**：在正常非沙盒 Windows PowerShell 中运行 `.\build.ps1 -Test -Case CTREE`，确认 `taskkill /PID ... /T /F` 干净清除进程树且无 `临时目录清理失败` 告警。
- [ ] **主程序 `CodeVaultUI.exe` 异地试打包**：由 C 使用 `windeployqt` 收集 `CodeVaultUI.exe` 的 Release 依赖与插件，合并 B 的完整 `toolchain/`（源自 `mingw1510_64`）和样例 `samples/`，复制到全新英文可写目录，脱离开发路径启动完成注册、登录、导入与真实多点判题。
