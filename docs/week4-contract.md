# CodeJudge Local 第四周公共约定与发布规范（成员 B 统筹）

> 适用成员：A（题库、筛选与进度）、B（判题引擎、工具链与统筹）、C（账号、主窗口与部署）  
> 本文依据[第四周实施计划](../../CodeVault_五周计划第四周.md)与第三周公共约定整理，记录第四周接口边界、成员 B 对判题与工具链的加固约定，以及候选发布目录布局。

---

## 一、三人共同约定摘要

1. **账号与进度身份**：
   - C 分配稳定内部 ID（如 `u0001`），界面显示 `username`（3～20 位英文字母、数字或下划线，区分大小写并查重）。
   - 进度继续按 `userId + packageId + problemId` 三个字段分别比较定位；第三周旧有 `practice` 记录留在原身份下，不自动迁移到新注册账号。
2. **会话锁定与防冲突**：
   - 登录后才开放练习和进度；发起判题时锁定本次 `userId`、题包 ID、题目 ID 和原源码路径，任务完成时按该锁定身份写进度。
   - 判题期间禁止退出登录、切换账号、移除或导入共享题包。
3. **搜索、移除与重导入**：
   - 搜索支持标题子串（区分大小写）、单标签精确匹配、题包 ID、当前账号状态（`-1` 表示不限），多个条件同时满足，空条件表示不限。
   - 移除仅隐藏整个共享题包，提交移除标记成功后刷新可见列表，保留缓存与所有账号进度。
   - 重新导入同 ID 同内容题包时恢复可见并保留原进度；同 ID 内容有变化时拒绝并提示使用新 ID。
4. **应用路径定位**：
   - 从可执行程序所在目录（`QCoreApplication::applicationDirPath()`）推导 `data/`、`toolchain/` 与 `work/`，软件与临时工作目录必须放在可写英文目录下。

---

## 二、A / C 对外接口摘要（由各自负责人维护）

> 成员 B 不直接修改 `src/common/types.h`、`src/problem/`、`src/progress/`、`src/account/` 与 `ui/`；以下签名供三方对齐核对。

- **成员 A（`types.h` / `ProblemLibrary`）**：
  - `struct ProblemQuery { std::string titleKeyword; std::string tag; std::string packageId; int status = -1; };`
  - `std::vector<Problem> searchProblems(const ProblemQuery& query, const std::vector<Progress>& currentUserProgress);`
  - `bool removePackageFromLibrary(std::string packageId, std::string& error);`
- **成员 C（`AccountManager`）**：
  - `bool load(std::string& error);`
  - `bool registerAccount(const std::string& username, const std::string& password, std::string& error);`
  - `bool login(const std::string& username, const std::string& password, std::string& userId, std::string& error);`

---

## 三、成员 B 判题引擎与工具链加固约定

### 1. 工具链故障与源码 `CE` 区分
在 `JudgeEngine` 编译阶段正常退出且退出码非 `0` 时，先调用 `isToolchainFailure()` 检查编译输出：
- 若输出全为空白（常见于缺系统 DLL 导致异常非零退出），或包含明确的工具链内部调度/安装异常关键词（`cannot execute`、`cannot exec`、`CreateProcess`、`installation problem`、`error trying to exec`、`cannot find 'ld'`、`cannot find 'as'`、`cannot find 'cc1plus'`、`internal compiler error`），一律判定为 **`InternalError`** 并提示检查编译器目录完整性，**不再误判为学生源码 `CE`**。
- 仅当存在正常源码诊断输出且非工具链自身异常时，才判定为 **`CE`**。

### 2. 编译与运行双阶段 `PATH` 隔离
- 在 `startCompile()` 和 `startRun()` 两个阶段，均将 `compilerPath_` 所在目录（即 `<appDir>/toolchain/bin`）插入子进程 `PATH` 环境变量的**最前面**。
- 作用：确保在未配置系统 `g++`（或系统中存在其他版本 MinGW）的机器上，不仅编译器能找到配套子程序，编译生成的学生程序 `main.exe` 也能优先加载配套版本的 `libstdc++-6.dll`、`libgcc_s_seh-1.dll`、`libwinpthread-1.dll`，防止因版本冲突或缺运行库出现 `RE`。

### 3. 编译超时清理与测试调参接口
- 保留 `JudgeEngine(std::string compilerPath, QObject *parent = nullptr)` 原签名与默认 `30000` 毫秒编译超时不变。
- 新增测试辅助方法 `setCompileTimeoutMs(int timeoutMs)`（仅供单元测试加速复现编译超时，C 无需调用）。
- `terminateCompileTree()` 在 `taskkill /PID <PID> /T /F` 超时时显式清理 `killer` 进程并附带失败详情，再由 `process_->kill()` 兜底收尾，防止局部对象析构卡死。

### 4. 内置工具链 `toolchain/` 选型与目录清单
- **实测排查结论**：工作区根目录原有的 `toolchain.zip`（2018 年 MinGW-W64 GCC 8.1.0）在当前环境下运行 `g++.exe --version` 与 `gcc.exe --version` 均报 `{Illegal System DLL Relocation}` 错误，不可直接用于发布。
- **正式选用工具链**：使用与当前 Qt 6.12 构建套件同源的 **MinGW-W64 GCC 15.1.0**（本机源路径 `D:/Qt/Qt/Tools/mingw1510_64`，含完整编译、链接、标准库头文件与运行库，总大小约 702 MB）。
- 发布目录内的 `toolchain/` 需完整保留以下子目录与文件：
  - `toolchain/bin/`（含 `g++.exe`、`as.exe`、`ld.exe`、`libstdc++-6.dll`、`libgcc_s_seh-1.dll`、`libwinpthread-1.dll` 等）
  - `toolchain/libexec/`（含 `gcc/x86_64-w64-mingw32/15.1.0/cc1plus.exe` 等编译前端子程序）
  - `toolchain/include/`、`toolchain/lib/`、`toolchain/x86_64-w64-mingw32/`、`toolchain/etc/`、`toolchain/opt/`、`toolchain/share/`
  - `toolchain/licenses/`（发行许可证目录）与 `toolchain/build-info.txt`

---

## 四、候选发布目录结构约定（B / C 试打包参考）

部署到纯英文可写目录（例如 `D:/CodeVaultRelease/`）后，目录布局约定如下：

```text
CodeVaultRelease/
├── CodeVaultUI.exe             C：Release 构建的主程序
├── Qt6*.dll / platforms/ ...   C：使用匹配 Kit 的 windeployqt 收集 Qt 依赖与插件
├── toolchain/                  B：完整 MinGW 15.1.0 工具链（含 bin/g++.exe 与 licenses/）
├── samples/                    A/B：演示题包与测试源码样例
├── data/                       运行时生成（题库缓存、progress.txt、accounts.txt、removed_packages.txt）
└── work/                       运行时生成（每次判题创建唯一 judge-XXXXXX，完成后清理）
```
