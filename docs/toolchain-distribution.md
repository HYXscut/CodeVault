# 内置编译工具链清单、自检与分发许可说明（成员 B 交付）

> 适用阶段：第四周试打包、第五周最终发布包整合与跨电脑验收。

---

## 一、固定工具链版本与构建参数

- **选用套件**：MinGW-W64 GCC **15.1.0**（`x86_64-posix-seh-rev0, Built by MinGW-Builds project`，与 `CodeVaultUI` 使用的 Qt 6.12.0 `mingw_64` Kit 同源）
- **开发机源路径**：`D:/Qt/Qt/Tools/mingw1510_64`（总体积约 702 MB）
- **应用内相对定位**：`<发布根目录>/toolchain/bin/g++.exe`（由 `QCoreApplication::applicationDirPath()` 拼接得到）
- **判题固定编译参数**：`-std=c++17 main.cpp -o main.exe`
- **工作环境变量隔离**：在 `JudgeEngine::startCompile()` 与 `JudgeEngine::startRun()` 中，均把 `<发布根目录>/toolchain/bin` 插入子进程 `PATH` 最前面，确保在未安装系统 `g++`（或存在其他旧版本 MinGW）的电脑上都能加载匹配运行库。

> **为什么不用仓库外部旧版 `toolchain.zip`？**  
> 实测工作区早期的 `toolchain.zip`（2018 年 GCC 8.1.0）在当前 Windows 环境下执行 `g++.exe --version` 会直接报 `{Illegal System DLL Relocation}` 无法启动，故正式发布统一采用同源的 GCC 15.1.0。

---

## 二、必须保留的工具链目录与关键文件

发布包内的 `toolchain/` 必须完整拷贝整个目录树，不可只单独复制 `g++.exe`：

| 目录 / 文件 | 作用说明 | 缺失时的典型表现 |
|---|---|---|
| `toolchain/bin/g++.exe` | 编译驱动入口与版本检查目标 | `CompilerChecker` 报“找不到指定编译器”，`startJudge` 直接拒绝 |
| `toolchain/bin/as.exe`、`ld.exe` | 汇编器与链接器 | 无法生成 `main.exe`，触发 `InternalError` |
| `toolchain/bin/libstdc++-6.dll`、`libgcc_s_seh-1.dll`、`libwinpthread-1.dll` | C++ 标准库与线程运行库（编译器与学生 `main.exe` 共用） | 无系统 `g++` 的电脑上运行 `main.exe` 闪退或无法启动 |
| `toolchain/libexec/gcc/x86_64-w64-mingw32/15.1.0/cc1plus.exe` | C++ 编译前端子程序 | `g++.exe` 报 `cannot execute 'cc1plus': CreateProcess: No such file or directory`，引擎判定为 `InternalError` |
| `toolchain/lib/gcc/x86_64-w64-mingw32/15.1.0/include/c++/` | C++ 标准库头文件（如 `<iostream>`、`<vector>`、`<string>`） | 编译学生代码时找不到标准头文件 |
| `toolchain/x86_64-w64-mingw32/include/` 与 `lib/` | Windows API / C 运行时头文件与链接库 | 编译或链接报错找不到系统库 |
| `toolchain/build-info.txt` | 工具链构建配置与版本溯源记录 | 缺失不影响运行，但发布包留档建议保留 |
| `toolchain/licenses/` | 随工具链附带的 26 项开源组件原始许可文件 | 分发合规必备，同时在发布根目录 `licenses/toolchain/` 汇总一份 |

---

## 三、分发与第三方开源许可说明（`licenses/`）

根据第五周要求，发布包根目录需包含 `licenses/`，汇总工具链第三方组件的原始许可文件。`mingw1510_64/licenses/` 包含以下 26 个组件目录的授权文本：

- **编译器与链接器核心**：`gcc`（含 `COPYING3` GPLv3、`COPYING.RUNTIME` GCC Runtime Library Exception 等）、`binutils`、`mingw-w64`（含 `COPYING.MinGW-w64.txt`、`COPYING.MinGW-w64-runtime.txt`、`DISCLAIMER`）、`winpthreads`
- **编译器依赖数学与压缩库**：`gmp`、`mpfr`、`mpc`、`isl`、`cloog`、`ppl`、`zlib`、`bzip2`、`xz`、`libiconv`、`libffi`
- **随附调试与工具组件**：`gdb`、`make`、`expat`、`mingw-libgnurx`、`ncurses`、`openssl`、`python`、`readline`、`sqlite`、`tcl`、`tk`

**分发合规提示**：
1. 本软件以独立进程（`QProcess`）调用 `toolchain/bin/g++.exe` 编译学生源码，不将 GCC 源码静态编入 `CodeVaultUI.exe` 自身；
2. 随发布包一并分发工具链二进制时，必须保留 `licenses/` 目录下上述全部原始许可与声明文件（含 `COPYING.RUNTIME` 运行库例外条款）；
3. Qt 6 框架自身的运行库（`Qt6Core.dll`、`Qt6Widgets.dll`、`Qt6Network.dll` 等）及配套许可，由成员 C 在执行 `windeployqt` 收集 Release 依赖时一并核对归入发布包。

---

## 四、一键复制与脱离开发机自检命令

成员 B 与成员 C 组装候选发布目录（例如 `D:/CodeJudgeLocal_v1`）时，可执行脚本：

```powershell
# 1. 复制完整工具链与 licenses/ 到候选发布目录，并立即在剥离开发 PATH 的环境下真编译自检
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\prepare-toolchain.ps1 -TargetDir "D:\CodeJudgeLocal_v1"

# 2. 若已复制好目录，仅重新核对完整性与真编译自检（不重复拷贝 702MB 文件）
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\prepare-toolchain.ps1 -TargetDir "D:\CodeJudgeLocal_v1" -VerifyOnly
```
