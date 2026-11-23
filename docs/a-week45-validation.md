# 第四、五周 A 验收与总联调

## 当前证据与边界

本机 Qt 6.12.0 / MinGW 15.1 Release 主程序编译链接通过。A 隔离测试 58 项通过；账号测试 49 项通过；注册窗口离屏交互与布局检查通过。真实题包联测使用新注册的两个内部账号 ID，7 次 AC、WA3、CE、RE、TLE、OutputLimit、AC 的状态和计数正确，逐次完成信号为 1、引擎空闲、work 任务目录清理、重载账号/进度及移除恢复通过。

这些结果不等于最终 GUI/跨电脑验收通过。B 原有 50 次回归记录保留，不把本轮 7 次称为又完成 50 次。最终使用相同版本的发布包，在第二台 Windows x64 电脑复核 `week5-checklist.md`。

## 可复跑入口

共享源码位于 `examples/a_validation/`；`tests/` 在仓库忽略规则中，本轮不修改该规则。以下在 PowerShell 执行，按实际安装位置调整 Qt 根目录，使用英文构建路径：

```powershell
$env:PATH = 'D:\Qt\Tools\mingw1510_64\bin;D:\Qt\Tools\Ninja;D:\Qt\6.12.0\mingw_64\bin;' + $env:PATH
& 'D:\Qt\Tools\CMake_64\bin\cmake.exe' -S D:\CodeVault\examples\a_validation -B D:\CodeVault_A_validation\shared-tests -G Ninja '-DCMAKE_PREFIX_PATH=D:/Qt/6.12.0/mingw_64'
& 'D:\Qt\Tools\CMake_64\bin\cmake.exe' --build D:\CodeVault_A_validation\shared-tests -j 4
& D:\CodeVault_A_validation\shared-tests\ProblemLibraryCheck.exe
& D:\CodeVault_A_validation\shared-tests\AccountCheck.exe
$env:QT_QPA_PLATFORM = 'offscreen'
& D:\CodeVault_A_validation\shared-tests\LoginDialogCheck.exe
Remove-Item Env:QT_QPA_PLATFORM
& D:\CodeVault_A_validation\shared-tests\ProblemJudgeIntegration.exe 'D:\Qt\Tools\mingw1510_64\bin\g++.exe'
```

每条命令检查退出码为 0；构建失败时不要运行旧 exe 冒充新版本。离屏测试使用测试用系统字体，正常桌面的布局仍应手工核对。真实联测可将最后一个参数换成候选包内的 g++.exe，验证发布工具链。

## 正式题包与 50 道筛选记录

正式演示按 `problem-package-format.md` 使用已有四题。筛选专用题包是 `samples/filter_validation/filter_a` 和 `filter_b`，每包 25 题，均有 P001～P025；偶数题标题 Loop n，标签 loop/basic，奇数题标题 Array n，标签 array/basic。所有题测试输入 1 2、输出 3，只用于检查筛选，不表示 50 道不同教学题。

自动测试内存中的 Alice 记录：filter_a/P001 本地通过、filter_a/P002 练习中、filter_b/P001 练习中；Bob 仅 filter_a/P001 练习中。手工验证从空运行副本开始，注册 alice 和 bob，自选演示密码；导入两包后，用 answer_multi_ac.cpp 和 answer_wa.cpp 分别制造这些状态。未绑定题不要另存记录。

| 条件 | Alice 的明确预期 |
|---|---|
| 全空 | 50 题 |
| 标题 Loop | 两包所有偶数题，各 12 题，共 24 |
| 标题 loop | 0，标题区分大小写 |
| 标签 loop | 与标题 Loop 同一集合，共 24 |
| 标签 basic | 50 |
| 题包 filter_a | P001～P025，共 25 |
| 本地通过 | 仅 filter_a/P001 |
| 练习中 | filter_a/P002、filter_b/P001 |
| 未开始 | 除上述三题外的 47 题 |
| Loop + loop + filter_a + 练习中 | 仅 filter_a/P002 |
| Loop + loop + filter_a + 本地通过 | 0 |
| 清空后再点确定 | 恢复 50 |

Bob 本地通过为 0，练习中仅 filter_a/P001，未开始 49。注意清空按钮只重置对话框控件，点确定后才应用。

## 总联调顺序

1. 从空数据候选目录启动。注册两账号，检查非法用户名、重复用户名、两次密码不一致、错误密码；同密码两账号在文件中应有不同盐，文件不含原密码。重启可登录。
2. 登录 alice，导入 demo 与 week3_demo，共四题；点编译器检查，随后五点题绑定 answer_multi_ac.cpp，真实 AC 为 5/5/5。改绑 answer_wa_at3.cpp，WA 为 2/3/5，第 4、5 点 NotRun。CE 为 0/0/5。
3. 继续 RE、TLE、OutputLimit，各后立即重测 AC。判题时清除源码、退出、筛选、导入、移除、切换题目不可用；完成后恢复。检查 work 下无 judge-* 任务残留。
4. alice 最后 AC，退出登录；个人题面、路径和结果应清空。bob 同题未开始，绑定自己的 WA 源码后判 WA。重启并依次登录，alice 为本地通过，bob 为练习中，路径和摘要分别恢复。
5. 清空筛选，移除 week3_demo，重启后两账号均看不到。重导入原包恢复，各自进度仍在。另复制原包到临时目录，只修改 statement.md，再尝试恢复，应拒绝；还要分别检查元数据、测试输入/答案改动。不要改正式 samples 原件。
6. 在另一个空运行副本导入筛选两包，建立上表状态并核对命中集合。空结果不能显示上一轮的旧列表，清空条件可恢复。
7. 关闭应用，复制整个候选目录到另一英文路径，启动后核对题库、隐藏标记、账号和进度。原源码若不在移动范围内仍用旧路径；另行改名源码，选题时应提示失效，重新绑定后可判。
8. 最后在未配置系统 g++ 的第二台 Windows x64 电脑，使用同一份包重复上述核心链路，记录包名、exe SHA-256、设备、结果和未通过项。

## 异常恢复：只操作隔离运行副本

先关闭隔离应用，备份其中的整个 data。外部坏题包也从 samples 复制，逐项单独修改后测试：选错父目录、缺 1.in、缺 1.out、题号重复、包重复、时限 0、声明一个点但多出 2.out。预期导入失败，错误有具体原因，原有题库数量和有效进度不变。

进度损坏、隐藏标记损坏分别单独测试。关闭后用有效备份恢复，勿靠删除文件解决。账号旧格式转换见 account-format.md；不要把真实账号或 data 放进源码仓库。

独占文件锁使用另一个 **PowerShell** 窗口，先把路径改为隔离应用的真实文件路径。运行程序已加载并绑定源码后再持锁：

```powershell
$lockPath = 'D:\CodeVault_A_validation\candidate_moved\data\progress.txt'
$heldFile = [System.IO.File]::Open($lockPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::None)
```

保持该窗口不关闭，在应用中判 WA：结果应可见，保存失败须有提示，旧有效记录保持。检查完回到持锁窗口执行：

```powershell
$heldFile.Dispose()
```

用备份和解锁后的原文件执行 `fc.exe /b <备份路径> <文件路径>`，应无差异。重新启动确认旧状态；解锁后重判 WA 应正常保存。同样办法锁 removed_packages.txt 后移除，应失败且仍可见；锁账号文件后注册应失败，解锁后重启不能登录失败注册的账号。

记录表：

| 项目 | 候选版本/文件哈希 | 结果与截图位置 | 失败复现/重测 |
|---|---|---|---|
| GUI 双账号与五点判题 | rc1～rc4 | AC/WA、路径状态摘要及重启恢复由用户反馈通过 | 用户补充确认 CE/RE/TLE/OutputLimit 状态和计数正确，异常后可再次 AC |
| GUI 50 题组合筛选 | rc3～rc4 | 标题/标签/包/状态/交集/空结果通过；含原题共51题 | 用户手工反馈，无逐项截图 |
| GUI 移除与恢复 | rc3 | 重启隐藏、三类改内容拒绝、原包恢复和两账号原进度保留通过 | 导入失败明确弹窗补验通过 |
| GUI 进度保存失败 | rc4 | 锁文件后WA结果可见、保存报错、旧文件无差异、重启保留、解锁恢复通过 | 用户手工反馈 |
| GUI 坏题包与进度损坏 | rc4 | 七项错误导入拒绝；损坏进度拒绝且不覆盖，恢复后正常 | 用户手工反馈 |
| 同机整体移动 | rc4 | 账号、51题、编译器与新位置重绑定判题通过 | 当前工作候选目录为candidate_moved |
| 横向滑块显示 | rc4 | 用户确认拖到右侧后可完整看到状态列 | 通过，用户手工反馈 |
| 第二台电脑完整流程 | rc4 | 待验收 | 用户暂时没有可用电脑，明确保留待办 |

本机自动结果与用户手工反馈分开记录。先保存不含个人data的候选ZIP；跨机及剩余项通过后再固定最终交付ZIP。

## 完整主窗口会话专项

用户报告切回另一账号时源码路径和摘要变化，因此不能仅据账号类、进度类独立通过就认定窗口隔离完全通过。新增独立工程 examples/a_validation/session，构建后用SessionIsolationCheck执行真实MainWindow登录、OutputLimit判题、退出、另一账号登录和回切，检查原源码路径、摘要及文件归属。该测试使用独立合成数据，拒绝覆盖已有测试data；需要在测试exe旁提供匹配工具链。独立工程用于避免多个Qt UI测试目标共用生成文件的冲突。

本次该专项通过，尚未复现用户所报跨账号变化。rc2增加主界面正文/结果的账号显示和启动前身份一致性检查。用户候选中的旧bob记录需由用户重新判WA建立对照，不由修复程序擅自改写；随后在明确显示alice的会话判题、切回bob复测。后续rc2现场复测获用户反馈通过；原异常根因仍未确认，不能声称定位了自动串写。


## 当前交付状态

本机A核心数据流程已完成上述检查，现可生成空data的rc4候选ZIP。原candidate_moved的账号、源码绑定和进度只作为本机验收数据保留，不复制到待分发包。横向滑块效果与GUI异常状态已获用户单独确认通过；另一台Windows x64实测仍为待办，不能将第五周最终验收整体打勾。
