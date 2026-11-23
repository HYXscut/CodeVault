# 第五周成员 B：工具链自检、核心回归与跨电脑验收记录

> 登记范围：成员 B 负责的判题模块、编译检查与便携工具链验证已全部实测通过；主窗口、账号及第二台电脑跨机验收表已就绪，待 A、C 代码合入打成最终发布 ZIP 后现场逐项勾选填写。

---

## 一、版本与环境基线登记

| 记录字段 | 当前确认信息 |
|---|---|
| 仓库基线提交 | `4ee00c6`（叠加成员 B 第 4/5 周加固：本地已改 `src/judge/judge_engine.h`、`src/judge/judge_engine.cpp`） |
| Qt 构建 Kit | Qt 6.12.0（`mingw_64`），CMake 3.30.5，Ninja 1.12.1 |
| 固定内嵌工具链 | MinGW-W64 GCC **15.1.0**（`x86_64-posix-seh-rev0`，源路径 `D:/Qt/Qt/Tools/mingw1510_64`，含完整 `licenses/`） |
| 成员 B 交付文档 | [week4-contract.md](week4-contract.md)、[week4-checklist.md](week4-checklist.md)、[toolchain-distribution.md](toolchain-distribution.md)、[judge-module-design.md](judge-module-design.md)、[week5-checklist.md](week5-checklist.md) |
| 可复跑测试与准备脚本 | `tests/CMakeLists.txt`、`tests/judge_engine_test.cpp`、`build.ps1`、`tests/prepare-toolchain.ps1` |

---

## 二、成员 B 已完成自检记录（本机开发与隔离模拟实测）

| 自检项目 | 执行命令 / 条件 | 预期结果 | 实测结果 | 结论 |
|---|---|---|---|---|
| 全量回归与新增第 5 周专项 | `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Test -Case ALL` | 20 轮混合判题 + 异常/边界/中文路径/目录不可写/编译超时进程树全通过，退出码 `0` | `20/20` 基础通过；`MissingCompiler`、`BrokenToolchain`、`Boundary 6/6`、`BundledToolchain 3/3`、`ChineseAndSpaceSource`、`WorkDirUnwritable`（拒绝并恢复 `AC`）、`CompileTimeoutTree` 全部 `PASS`（注：沙盒中 `taskkill` 超时由兜底安全接管） | **通过** |
| 中文目录与带空格源码路径 | `ALL` / `WEEK5` 中构造 `中文 源码目录/我的 A+B 解答.cpp` | 正确编译运行 `AC`，`result.sourcePath` 原样保留中文空格路径，原文件完好 | `Status: AC`，`Source intact: true`，完整保留原 UTF-8 路径 | **通过** |
| 工作目录不可写拒绝与恢复 | `ALL` / `WEEK5` 中以同名文件占住 `work` 路径后再清理重测 | 返回 `false`，明确提示放到普通用户可写的英文目录，引擎保持空闲且恢复后可正常 `AC` | `Accepted: false`，提示包含 `"无法创建或写入工作目录，请将软件放到普通用户可写的英文目录"`，`Recovery AC: true` | **通过** |
| 50 次混合判题压力回归 | `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Test -Case MIX50` | 50 次混测全过，50 次通知，`work/` 残留 `0` | `Total passed: 50/50`（`AC=15, WA=15, CE=5, RE=5, TLE=5, OutputLimit=5`），`Total notifications: 50`，`Max residual dirs: 0`，`Final engine busy: false` | **通过** |
| 工具链分发核对与真编译自检 | `.\tests\prepare-toolchain.ps1 -TargetDir "<英文试包目录>"` | 核对 10 项核心文件 + `licenses/toolchain/gcc/COPYING3`，剥离系统开发 `PATH` 编译运行 `3+5` | 识别 `g++.exe ... 15.1.0`，`3 + 5 = 8` 通过并清理临时自检目录 | **通过** |
| 脱离开发 `PATH` 便携运行 | `portable_demo/JudgeEngineTest.exe --bundled TOOLCHAIN`（仅保留系统 `PATH`） | 自动拼出 `<appDir>/toolchain/bin/g++.exe`，`CompilerChecker` 与 `JudgeEngine` 均正常 | `Compiler found: true`（`15.1.0`），`AC → CE → AC` `3/3` 通过，`Max residual dirs: 0` | **通过** |

---

## 三、最终发布包与第二台 Windows x64 电脑验收表（合入 A/C 后现场填写）

### 3.1 设备与版本信息（验收时填写）

| 字段 | 填写内容 |
|---|---|
| 最终发布包名称与版本 | `CodeJudgeLocal_<版本号>_Windows_x64.zip`（对应 Git 提交：________） |
| 匿名验收设备编号 | 设备 #____ |
| 操作系统与架构 | Windows ____（64 位 x64） |
| 解压运行目录（纯英文） | `D:/________` |
| 系统 `g++` PATH 状态 | 未配置系统 `g++`（`where g++` 找不到） |
| 普通桌面下 `CTREE` 复核 | 在非沙盒普通 PowerShell 执行 `.\build.ps1 -Test -Case CTREE`：□ 通过 / □ 待查 |

### 3.2 跨电脑 12 项逐项验收清单（对应第五周 §5.1）

- [ ] **启动与环境**：在未配置系统 `g++` 的电脑上，从普通用户可写英文目录直接双击启动 `CodeVaultUI.exe` 成功。
- [ ] **内置工具链自检**：界面点击检查编译器正常显示 `g++ 15.1.0`；用临时缺件副本能弹出可读错误提示。
- [ ] **账号注册与登录**：注册、登录甲乙两账号，测试重复用户名、错误密码、退出重登均正常。
- [ ] **题包导入与中文源码**：导入正式 `samples/` 题包，显示题面并重启恢复；用含中文与空格路径的 `.cpp` 完成判题且原文件未受改动。
- [ ] **五点判题与三项计数**：真实复现 `AC`（`5/5/5`）、第 3 点 `WA`（通过 `2` / 已执行 `3` / 总点数 `5`，后续 `NotRun`）、`CE`（`0/0/5`）、`RE`、`TLE`。
- [ ] **输出超限与连续可用**：输出超限显示独立状态 `OutputLimit`；`TLE` 与 `OutputLimit` 后立刻判下一题正常，`work/` 临时目录无残留。
- [ ] **进度保存与重绑定**：绑定、更换、清除原路径与摘要/时间重启可恢复；原 `.cpp` 移动后提示路径失效并允许重新绑定。
- [ ] **双账号隔离与忙碌锁定**：甲乙路径、状态及结果独立；退出登录清空个人区域；判题中禁止切换或退出账号。
- [ ] **组合筛选**：标题、单标签、状态、题包及组合条件查询结果准确，包含无结果与清空条件。
- [ ] **题包移除与恢复**：移除后重启仍隐藏；重新导入同内容题包恢复且保留两人原进度；同 ID 改内容题包被拒绝。
- [ ] **异常文件容错**：错误题包、损坏记录与不可写目录均给出明确原因，不崩溃且不损坏原有有效记录。
- [ ] **整体移动可迁移**：将整个发布目录移动到另一英文目录后启动，题库、账号、进度仍可读，并使用新目录内依赖与工具链正常判题。

### 3.3 若发现未通过项的登记模板

| 序号 | 发现问题与复现步骤 | 所属模块与负责人 | 修复后候选版本 | 重跑结果 |
|---|---|---|---|---|
| 1 | | | | |
