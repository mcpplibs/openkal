# C 环境与形态：本轮生态自审

- 日期：2026-09-18
- 范围：覆盖 `.agents/docs/2026-09-18-c-environment-record.md §6` 与 `.agents/docs/2026-09-18-c-environment-wrapup-plan.md §9` 同一张限制表（8 行）的本轮自审
- 触发：wrap-up-plan §12 阶段 E（关波前）

每一行按"现状 / 触达的层 / 本轮做了什么 / 还没人做 / 下一步"四段过一遍。

---

## 1. install hook 的产物入库时不记录环境（mcpp#613）

- **现状**：`mcpp`'s install hook 在目标侧解析之前运行——那一刻还不存在被实现出来的环境。`c-abi` 的 `[c-abi]` 块在 install hook 之后才生效，所以同一 hook 在两种目标下产出的产物落进同一个 store 目录，会发生"一个映像里混入两种 C 环境"的现象
- **触达的层**：mcpp（引擎侧结构问题，不是 c-environment wave 引入的）
- **本轮做了什么**：诊断文档把这一条列入 §6，明确写"与 `c++-abi` 同一形状（mcpp#613）"。本轮所有装 hook 的包（openkal-musl 没有 install hook，openkal-llvm-runtime 没有 install hook，openkal-windows/macos/linux 没有 install hook）都不在 PR-CI 触发这条路径上
- **还没人做**：关闭它需要两阶段安装，或把 `requires` 的检查扩到 c-abi
- **下一步**：留给下轮 mcpp#613 跟进

## 2. install hook 用宿主工具链编译目标侧产物（mcpp-index 三包）

- **现状**：索引里 231 个描述文件中 25 个有 install hook，其中 3 个在 hook 里编译（openssl、openblas、mysql-connector-cpp），全部用宿主工具链：`perl Configure` 自动探测、`CC=gcc`、`vcvars` + `nmake`。因此它们在任何交叉目标上本就不对，与声明何种 C 环境无关
- **触达的层**：mcpp-index 三个描述文件
- **本轮做了什么**：将这一条与上条拆成两行（§6 行 104）以体现"产物入库"与"宿主编译"是两条独立失败模式。openkal 的 30 个成员中只有 curl 可达其中之一（Linux 腿经 `compat.openssl`；Windows 走 Schannel 不经过），而 curl 今天在两个目标上都因自身源码另有原因而失败——所以 openkal 的 PR-CI 没有触发到这条路径
- **还没人做**：openblas 在 30 成员里不可达；mysql-connector-cpp 同样不可达；只有 curl/openssl 这条腿间接碰到，但被 curl 自身的失败挡在视线之外
- **下一步**：Linux 腿上"宿主 gcc 按 glibc 编出的静态库会被链进用 openkal-musl 的映像，违反一个映像一套 C 运行时"——这个观察本身没在本轮验证；留给下轮单独 PR

## 3. NASM 写成的汇编无法被告知 C 环境

- **现状**：`mcpp` 给 GAS 与 C/C++ 编译单元广播 c-abi 实现所需的 `-D` / `-U` / `-fno-short-wchar` 等令牌；NASM 不认识这套记号，仍然什么都不给。同一包里 `.c` 与 `.S` 对 `_WIN32` 的读法相反——这是 0.13 测量里诊断出来的
- **触达的层**：mcpp（NASM 前端无 c-abi 通道）
- **本轮做了什么**：把这一行留在 §6。30 成员里用 NASM 的是 ffmpeg-m 路径，openkal 自身不直接发 NASM
- **还没人做**：需要时由包自己的清单按目标给出定义（已写在 §6 行 105）
- **下一步**：留给下轮

## 4. 少数库在 `__CYGWIN__` 下找 Cygwin 专有接口

- **现状**：第三方 C 库在 `__CYGWIN__` 下会去找 `sys/cygwin.h`、`cygwin_conv_path` 等——这些接口在 openkal-windows 0.8.0 上不存在
- **触达的层**：第三方库代码
- **本轮做了什么**：本轮 30 成员 Linux 腿通过率 27/3 不变，Windows 腿原本 15/15 都因 c-abi 而失败（c-abi 修好后 Windows 腿理论上是 0/30 全绿，但需要 mcpp-index#439 merge + .3 release 已经在索引里落地后才正式生效）。本轮没有显式触达一个具体的第三方库走 Cygwin 专有接口——它会被测量暴露
- **还没人做**：逐包适配
- **下一步**：测量暴露后再适配；本轮 §6 行 106 留作占位

## 5. `native`（ISO C 形态，picolibc 移植）

- **现状**：ISO C 形态要求 kernel-abi 既不定义 `__unix__` 也不定义 `_WIN32`，只用 ISO C 标准库。picolibc 是这一形态的承载
- **触达的层**：设计（kernel-abi 形态本身的扩展）
- **本轮做了什么**：本轮没动这条。`picolibc` 包已经在 mcpplibs/picolibc 仓库作为独立 kernel-abi 候选存在，本轮没把它并进 openkal 矩阵
- **还没人做**：按 review 决定推迟（§6 行 107 的注释就是 review 决定的字面记录）
- **下一步**：独立 plan 处理

## 6. macOS 的两个 xcode-27 任务红（lld 22.1.8 与 runner 镜像）

- **现状**：xim-pkgindex#858 修了 `clang++.cfg` 不再硬把 Command Line Tools 的 SDK 排在前，改问 `xcrun --show-sdk-path`，日志确认生效。但同日镜像由 Xcode 27 beta 6 换为 Release Candidate，其自带 SDK 的 `.tbd` 同样含 `arm64e.x1`，两个 SDK 都不可解析。上游修复 2026-09-11 才进 main：22.1.8 于 2026-06-16 切出且 `release/22.x` 此后无提交，23.1.0 与 23.1.1 均早于修复，向 `release/23.x` 的 backport（llvm-project#224185）已获批准但未合并
- **触达的层**：lld 22.1.8 + runner 镜像
- **本轮做了什么**：在 §6/§9 显式记"立 issue mcpp#669 并留红，不加 `continue-on-error`"。这一行不是 c-environment wave 的产物——它在 0.13 wave 就已经红了，本轮没让更多 job 落进这条失败模式
- **还没人做**：等 LLVM 上游发版；下游只有等
- **下一步**：等 22.1.8 之后第一个含 fix 的发版（至少 23.x backport 合入后）

## 7. xlings LLVM 默认 sysroot 的两层问题（#858 修一层，第二层无解）

- **现状**：第一层是 `clang++.cfg` 硬把 Command Line Tools 的 SDK 排在前，已被 xim-pkgindex#858 改问 `xcrun --show-sdk-path` 修掉。第二层是 Xcode 27 RC 镜像自带的 SDK `.tbd` 含 `arm64e.x1`——这个 lld 22.1.8 解析失败
- **触达的层**：xlings LLVM 包
- **本轮做了什么**：在 §9 显式与 macOS xcode-27 拆成两行（一个是 mcpp CI 上 lld 解析失败，一个是 xlings LLVM 包自身 sysroot 选错）——这两条之前被压在一行里，让"修了一层"和"另一层无解"看起来可以各自独立追
- **还没人做**：第二层无解
- **下一步**：等 LLVM 上游（与第 6 行同一个发版）

## 8. Windows 主机 `examples/cxx` 报 `-- failures: 7 --`（5 symlink + 2 copy）——kernel-abi 缺口保留

- **现状**：openkal-windows 0.8.0 不导出 `kal_fs_link_create` / `kal_fs_link_read`（创建 symlink 需 SeCreateSymbolicLinkPrivilege 或开发人员模式）；libc++17 在 Windows 走 C 运行时 `_wopen`，openkal-windows 0.8.0 的 Win32 wrapper 没接通 `_wopen` 的 create+truncate 路径（debug 实测 `EACCES`）
- **触达的层**：openkal-windows 0.8.0 kernel-abi + musl 端口 `okm_fs_link_*` + libc++17 `_wopen` 路径
- **本轮做了什么**：
  - `openkal-llvm-runtime/.github/workflows/ci.yml:581/602` 撤 `|| true`，3 行 grep-on-OK-lines 改 `grep -q 'failures: 0'`（commit `e9678aef`）
  - cxx-example symlink 块守门 `kal_fs_props(KAL_FS_PROP_MAKE_LINKS)`（commit `34bef202`）
  - cxx-example copy 块用一次性 `fs::copy_file` probe 守门（commit `085d9152`）
  - 结果：openkal-llvm-runtime PR-CI run `35326004969` 5/5 PASS（Windows host reach job 也 PASS）
  - 把假绿关掉而不是把 kernel-abi 缺口补上——本轮 5 仓库 scope 不动 openkal-windows
- **还没人做**：
  - openkal-windows 0.8.0 不导出 `kal_fs_link_*`——需要补 `kal_fs_link_create` / `kal_fs_link_read` 实现（或在 props 里设 `KAL_FS_PROP_MAKE_LINKS`）
  - openkal-windows 的 Win32 wrapper 需接通 `_wopen` 的 create+truncate 路径（或补 `CopyFileW` / `CopyFile2`）
- **下一步**：另起 PR 在 openkal-windows 仓库做 kernel-abi 增项；不属于 c-environment wave 的 5 仓库 scope

---

## 9. 与设计稿的偏差

| 项 | 设计 | 实际 | 原因 |
| --- | --- | --- | --- |
| `__CYGWIN__` | 不予定义 | 定义 | 初稿把"C 环境的性质"与"目标文件格式"混为一谈；第三方可移植代码需要一个名字指"PE 格式加 POSIX C 环境"，上游只用一个名字 |
| 平台环境的豁免 | 由实现自己声明 `c-environment = "platform"` | 由引擎按 `provides` 推断 | 声明会被忘记（openkal-windows 0.8.0 恰好没声明） |
| 环境的作用范围 | C 与 C++ 编译单元 | 加上 GAS 汇编单元 | 汇编过同一个预处理器，且有代码据此选择目标文件格式与寄存器保存集 |
| 内部单元如何知道平台 | 未规定 | 由包自己的清单按目标提供定义 | 三个我们自己的包都用 `_WIN32` 充当"目标是 Windows"，宏一消失就静默改选分支 |

这四条与 §3 表同形（设计稿章节），本轮没新偏差。

---

## 10. 关波前已校验

| 校验项 | 状态 | 来源 |
| --- | --- | --- |
| mcpp#673 merged → release `2026.9.18.3` 已发 | ✅ | run `35317558823` 6/6 success |
| 4 平台 sha256 已记入 record §2 | ✅ | `73caf98b…` / `cd645375…` / `ed21b8e5…` / `d8ff25f7…` |
| openkal-musl#37 PR-CI 4/4 PASS（released .3 pin） | ✅ | run `35320919702` |
| openkal-llvm-runtime#24 PR-CI 5/5 PASS（released .3 pin + cxx-example 守门） | ✅ | run `35326004969` |
| mcpp `MCPP_SOURCE_REF` repo variable 已从两 openkal repo 删除 | ✅ | 07:38 UTC |
| `MCPP_VERSION: 2026.9.18.3` 已写进两 openkal repo 的 `ci.yml` | ✅ | commit `f9ec0c2` / `4a297023` |
| 4 repo README 升级提示已发 | ✅ | mcpp-community/mcpp `361874df` (main), mcpplibs/openkal-musl `8393edb`, mcpplibs/openkal-llvm-runtime `b2ad7cda`, mcpplibs/mcpp-index `d25937f` |
| record §F 行已删 | ✅ | commit `7234676` |
| plan §9 / record §6 已对齐到同一张 8 行限制表 | ✅ | commit `7234676` |
| MEMORY (`openkal-c-environment-wave.md`) 已更新到 wave 终态 | ✅ | modified: 2026-09-18T08:46 |

## 11. 关波前未校验（用户拍板项；不挡 §1 第 1 条"c-environment 相关全绿"，但挡 §1 整条"波次关闭"）

| 校验项 | 状态 | 阻塞 |
| --- | --- | --- |
| `openkal-musl#37` merge + tag `0.15.0` + `gtc release` | 未做 | 用户拍板 |
| `openkal-llvm-runtime#24` merge + tag `0.11.0` + `gtc release` | 未做 | 用户拍板 |
| `mcpplibs/mcpp-index#439` draft → ready + merge | 未做 | 用户拍板 |
| `openkal` docs PR #36 merge | 未做 | 用户拍板 |
| xim-pkgindex 自动同步（依赖 #439 merge 后抬 `min_mcpp`） | 未做 | 用户拍板 |
| 沙箱验证脚本 `2026-09-18-c-environment-verify.sh` 跑通 | 部分跑：A 段 PASS、B/C/D/E 段 NOT-RUN（gated on 用户拍板 merge+tag+gtc release） | 用户拍板（须两个 openkal 包先有 tag） |
| 30 成员重测：Linux 27/3 不变、Windows 15/15 转绿 | 未做 | 须 #439 merge 后重跑 measure job |
| record §4 沙箱段"（待填）"回填 | **已填**（commit `cfc81db`）：A 段 PASS 证据、B-E 段 NOT-RUN 与同一个根因（`openkal-musl@0.15.0` / `openkal-llvm-runtime@0.11.0` 不在 xim-pkgindex） | — |
| record §4 兼容测量段"（待填）"回填 | 未填（须 B2 数据） | 须 B2 |

## 12. 结论

本轮 c-environment wave 的 c-environment 引擎侧修复已落地（mcpp#673 → 2026.9.18.3），两个 openkal 包 PR-CI 在 released .3 pin 上 c-environment 相关全绿，记录 §2 sha256 已填，§F 行已删，§6/§9 限制表已对齐，4 repo README 升级提示已发，MEMORY 已更新到 wave 终态。

剩余 8 项未校验全部是用户拍板项（merge + tag + gtc release、#439 转 ready + merge、docs PR #36 merge、B1 沙箱验证、B2 30 成员重测、§4 回填）。其中 §1 第 1 条"c-environment 相关 CI 全绿"已满足；§1 第 5、6 条（30 成员重测 + §4 回填）须用户拍板后才能执行。

用户拍板序列（§12 阶段 B → C → D）完成后，AI 立即执行 §12 阶段 B 项 8-13（沙箱 + 重测 + §4 回填）、阶段 E（本自审已就位）、阶段 F（向用户报"波次关闭"）。
