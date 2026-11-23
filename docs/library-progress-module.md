# 题库、筛选与进度模块说明

`Problem`、`TestCase`、`Progress` 和 `ProblemQuery` 是纯数据结构体。`ProblemLibrary` 管理题包与可见性，`ProgressStore` 管理按账号区分的练习记录；两者通过公共类型与窗口、判题引擎协作，本身不参与继承。

| 类/接口 | 调用及作用 |
|---|---|
| ProblemLibrary::load | 启动/刷新时校验缓存题包和隐藏标记，重建路径 |
| importPackage | 新包导入，或逐字节核对后恢复隐藏包 |
| getProblems | 返回可见题目的副本 |
| findProblem | 按 packageId、problemId 找可见题；失败不改输出 |
| getTestCase | 读取 1～testCount 的真实输入和答案；全部成功才赋值 |
| searchProblems | 标题子串、精确单标签、精确包 ID、当前账号状态，条件取交集 |
| removePackageFromLibrary | 保存隐藏标记成功才改变可见性 |
| ProgressStore::getUserProgress | 先按当前内部账号 ID 取进度副本，再交给搜索 |
| getProgress / saveProgress | 按 userId、packageId、problemId 三项 ID 查询或替换 |

题库内部 `vector<Problem>` 保留全部缓存题；`vector<string>` 存隐藏包 ID。`data/removed_packages.txt` 每行一个题包 ID，无格式头；空行忽略，非法、重复或未知编号拒绝。无标记文件代表没有隐藏包，兼容原窗口的文件格式。题包和标记全部校验成功才启用题库；失败后禁止导入/移除，防止坏数据被覆盖。

搜索使用普通循环与 `string::find`，不使用 map、复杂模板或业务 Lambda。状态查询同时比较包 ID 和题号，无记录视为未开始。调用者必须传入 `getUserProgress(当前账号ID)` 的结果，不可把多个账号的记录混在一起。

Qt 限定在文件/窗口边界：`QString::fromUtf8()` 转换路径，`QFile` 读取字节，`QSaveFile::commit()` 提交候选数据。已有题包路径和复制辅助代码继续保留 `filesystem` 实现，本轮未重写其架构。新业务逻辑以结构体、vector、循环和 bool/error 为主。

窗口把控件条件装成 ProblemQuery；题目表格保存永久 ID，排序/筛选后仍按 ID 取题。开始判题时复制账号、题目、源码身份到 judgingProgress_；判题过程中锁定冲突操作，完成后按该副本保存有效结果。

课内对应：第 6 章结构体，第 7 章封装及对象组合，第 11 章 vector/string，第 12 章文件流与字符串流。项目整体的继承与多态由 B 的输出比较器体系体现，题库不为凑继承层次增加无意义基类。
