# openkal 生态 issue 定性 + mcpp-index 的 openkal 兼容测试与适配方案

- 日期：2026-09-17
- 范围：openkal #23 #24 #28 #31 #32；mcpp-index（231 个描述文件）
- 依据版本：openkal SPEC 0.12，openkal-linux 0.12.0（本地 `c445cba`），openkal-windows 0.7.0（本地 `b7de5e2`），openkal-musl README（本地 0.13.1），mcpp-index `0544619`
- 状态：分析稿，待 review。不改代码，不关 issue。

## 0. 结论摘要

1. **五个 issue 里有两个已经解决但还开着**：#23 (0.10 → 0.11 的 `KAL_SPAWN_BOUND_LIFETIME`) 和 #24 (0.10 的 `kal_fs_set_modified_at` / `kal_task_parallelism` / `kal_fs_capacity`)。SPEC §11.11–§11.13 都写了，issue 可以带着条款号关闭。
2. **真正值得进 ABI 的只有两处，而且都很小**：
   - `kal_err_not_executable`：一个错误值 (#28.3a)。ENOEXEC 现在被 openkal-linux 折成 `kal_err_io`，`execvp` 回退到 `/bin/sh` 的路径因此走不到。
   - "节点能否作为程序启动"这一个谓词：一个 props 位、一个 `present` 位、一个 setter (#31 / #32.1 / #28.3b)。**这不是 chmod，也不是权限模型**，§11.6 仍然成立。这是本批唯一需要你拍板的规范取舍。
3. **实现 bug**：openkal-windows 两个（spawn 继承了所有可继承句柄；非重叠 socket 让全双工连接死锁），openkal-linux 一个（错误码映射）和一个可选改进（stream 句柄避开 0，#28.2）。
4. **不属于 openkal**：#32.3 宿主 sysroot 泄漏是 mcpp 的 bug (mcpp#662)；#32.2 包里 include 了平台 SDK 头，是包 / 描述文件的问题。openkal 这边只需要补一份**信息性**文档，写清楚 openkal 目标上 C 包可以假设什么。
5. **按设计拒绝，写明理由后关闭**：位置 n 的描述符 (#28.1)、硬链接、命名管道、完整的 mode 字，以及 exec 后 pid 不同。
6. **mcpp-index 的 openkal 兼容测试**：测的是结果，不是作者的声明。结果写进单独的结果文件，由站点插件生成 `openkal` facet 和分类页；不在描述文件里加字段。它**依赖 mcpp#662 先修**，否则"通过"不可信。每个失败都按本报告 §1 的五类分诊，这就是今后 openkal issue 的来源。

## 1. 判定标准：一个诉求什么时候该进 openkal

openkal 的定位是"通用内核必要 ABI 的最小集合"，不是"让 POSIX 程序都能跑的兼容层"。SPEC 已经有一套可机械套用的准入规则，本报告只用它，不另立标准：

| # | 准入条件 | 出处 | 不满足时的去向 |
| --- | --- | --- | --- |
| A | **不可在线上组合**：在现有接口之上组合出来的结果会"让调用者静默出错"（simulation），而不是"只是受限"（supply） | §3.1 | 组合 → `openkal-kit` 或 C 库端口（openkal-musl） |
| B | **各环境自然具备**：实现不需要翻译表 / 注册表 / 名字解析器就能提供；形状不是从某一个环境借来的 | §7.1 | 不收；或者换一个各环境都自然的形状 |
| C | **可以先问再调**：只在部分资源/卷上成立的东西，要有 `*_props` 或 `present` 位来回答"这里有没有" | §6.2、§11.7/§11.10 的先例 | 不收，直到能表达"没有" |
| D | **最小增量**：优先一个 flag / 一个 present 位 / 一个错误值 / 一句规范文字，而不是新的一族操作 | §11.12、§11.14 | 收缩形状 |
| E | **缺了它的失败是静默的**：静默错误的优先级高于显式的 ENOSYS | §11.10、§11.13 的写法 | 显式失败可以排到后面 |

据此把每条诉求分到五类：

- **U — 使用侧问题 / 使用侧可解**：包描述、消费者代码、文档用法，不需要 openkal 动
- **T — 工具链 / 构建系统（mcpp）问题**：不是 openkal 的
- **I — 实现 bug**（openkal-linux / -windows / -macos / -musl）：规范已经说清楚了，实现没做到
- **S — 规范缺口，符合 A–E，建议补**：最小 ABI 增量
- **D — 规范按设计拒绝**：记录理由，给出替代写法，关闭

## 2. 逐条定性

### #23 — bound lifetime / execve 不可区分的说法

| 子项 | 类别 | 结论 |
| --- | --- | --- |
| 进程生命周期绑定到调用者 | **已解决（S，0.10 → 0.11）** | §11.11 + §11.14：0.10 `kal_process_spawn_bound`，0.11 改为 `KAL_SPAWN_BOUND_LIFETIME` flag + `KAL_PROCESS_PROP_BOUND_LIFETIME`。**issue 仍 OPEN，应关闭并链接 SPEC §11.11。** |
| execve 无法启动时调用者以 127 退出 | I（musl，已在 0.10.0 修） | 已由 musl 先 `kal_fs_info` 修掉 |
| 子进程看到的 pid 不是调用者的 | **D** | §7.1 不替换运行中的映像，这是 exec 组合后的固有差异；musl 在分歧表里写明即可 |
| process group 作为替代方案 | D（issue 自己已否决） | 同意：会把子进程移出终端前台进程组 |

### #24 — 目录 mtime / 处理器数 / 卷容量

三项**都已在 0.10 解决**：`kal_fs_set_modified_at`（§11.13）、`kal_task_parallelism`（§11.12，0 表示"说不出来"）、`kal_fs_capacity`（§11.13，按字节）。**issue 仍 OPEN，应关闭。**

issue 末尾顺带提的三项：
- 硬链接：**D**。并非各环境自然具备（FAT/UEFI ESP 没有），也没有静默失败 —— `link()` 显式失败即可。
- 命名管道：**D**。形状来自 POSIX 文件系统命名空间。
- 双向流对（`socketpair`）：**U/kit**。可以用两个 `kal_process_channel` 组合出来；若要 in-process 使用，放 `openkal-kit`（kit 的 channel 已经是这个位置）。

### #28 — 位置 n 的流 / 句柄为 0 / 节点能否执行

| 子项 | 类别 | 结论与建议 |
| --- | --- | --- |
| 28.1 在子进程 fd n（n>2）放一个流 | **D（按现形状）/ 远期 S** | "第 n 号描述符"是 POSIX 描述符表的形状，Windows 没有位置（CRT 靠 `lpReserved2` 模拟），违反 B。子进程还需要知道"去哪里找"，这正是 preopen 解决的问题。**若将来确有需要，形状应是"按名字授予的流"，类比 `kal_preopen`，而不是按编号的位置。**目前没有静默失败（musl 显式拒绝 `adddup2` n>2），按 E 排后。建议：记录到 §11 作为"已考虑，暂不定义"，关闭。 |
| 28.2 句柄值 0 同时表示"继承"和 stdin | **I（openkal-linux 可解）+ 已记录的规范缺陷** | `process.h` 已写明这个冲突，并允许实现"不对任何流查询回答 0"来消除歧义。最小做法是 **openkal-linux 把流句柄编码成 `fd+1`**（或别的非零编码），规范不用动。只有当第二个实现也碰到时，才考虑在规范里加一个哨兵值 `KAL_STREAM_INHERIT`（新增声明，§8 允许）。 |
| 28.3a 启动失败原因丢失（ENOEXEC → EIO） | **S（一个错误值）+ I（linux 映射）** | 评论里的测量很关键：内核的 ENOEXEC 通过 report pipe 回到了调用者，但在 `openkal-linux/src/sys.h` 的 `translate()` 里被 `default` 折成了 `kal_err_io`。调用者需要这个区分，因为 `execvp`/`posix_spawnp` 靠 ENOEXEC 决定是否回退到 `/bin/sh`，而 EIO 让这个回退不可达。**建议新增 `kal_err_not_executable = 14`**：满足 A（组合不出来）、B（Linux/macOS 是 `ENOEXEC`，Windows 是 `ERROR_BAD_EXE_FORMAT`，都是原生值）、D（一个值，按 §5.2 在 0.5 加过五个值的先例）、E（现在是错的，不是缺的）。linux / macos / windows 三个实现各加一行映射；musl `okm_errno` 加 `→ ENOEXEC`。同时修掉 musl README 和 issue 里"以 127 结束"这个过时说法（实际是 posix_spawn 返回 EIO）。 |
| 28.3b `access(X_OK)` 预先查询 | **与 #31 合并**，见下 | |

### #31 / #32.1 — 没有 chmod，无法产出可执行文件

**这是本批里唯一一个真正需要作取舍的规范问题。**

- 完整的 mode 字 / `chmod`：**D**。§11.6 的理由成立：权限以身份为前提，而各环境不认为身份存在；并且在不存 mode 的卷上 `chmod` "报告成功、什么都没做"，恰好是 openkal 要拒绝的结果。
- 但 issue 要的已经不是权限模型，而是**一个谓词：这个节点能否作为程序启动**。拿 A–E 逐条过：
  - A 组合不了：唯一的办法是 spawn `/bin/chmod`，依赖宿主 coreutils，而且无法验证。✓
  - B 自然：Linux/macOS 就是 `S_IXUSR|S_IXGRP|S_IXOTH`；Windows 的可执行性由格式/扩展名决定，**卷上不记录** → 不声明该属性。不需要任何表。✓
  - C 能先问：沿用 §11.7（链接）、§11.10（锁）的先例 —— "卷是否记录可执行性"是格式属性，由 `kal_fs_props` 回答。✓
  - D 最小：一个 props 位 + 一个 `present` 位 + 一个 setter，不是 mode。✓
  - E 静默：打包工具产出一个不能运行的 payload，直到上线才发现。✓
- 这与 §11.10 放在 §11.6 旁边的论证结构完全一致："缺的是一个词，不是一种能力"。

**建议形状（待你 review 决定）**：

```c
#define KAL_FS_PROP_EXECUTABLE  ((kal_uintptr)1u << 7)   /* 卷记录可执行性 */
#define KAL_INFO_EXECUTABLE     ((kal_u32)1u << 5)       /* present/wanted 位 */
/* kal_node_info 在尾部追加 int executable; —— 靠 self_size 机制向后兼容 */
int kal_fs_set_executable_at(struct kal_dir base, const char* name,
                             kal_uintptr len, int executable);
```

语义：只设置/报告"所有者能否执行"这一位（Linux/macOS 上 set 时同时设置 u/g/o 的 x 位，并遵循 umask 的等价规则由实现说明）；不声明 `KAL_FS_PROP_EXECUTABLE` 的卷返回 `kal_err_not_supported`，`present` 中该位留空 —— **不回答"是"**（issue 建议"没有此概念的平台回答 yes"，这恰好是 simulation，不采纳）。

需要你确认的冲突点：
1. ~~布局冻结是否允许追加~~ → **已查证可以（§6.1）**：SPEC §4.2 原文："`kal_node_info` is the one structure that is permitted to grow, and it carries `self_size` for precisely that reason"。§5.3 的正文没有引用这个例外，需要补一句。
2. §11.6 需要加一段"可执行性不是权限"的说明，否则读者会认为和 §11.6 矛盾。

### #32.2 — 包里 include 平台 SDK 头（`TargetConditionals.h`、`winsock2.h`）

| 部分 | 类别 | 结论 |
| --- | --- | --- |
| libarchive / tinyhttps 编不过 | **U**（index 描述文件 + 包源码） | 包按"目标是 macOS/Windows"写了平台分支，但在 openkal 图里 C 环境是 musl 头。修在描述文件或包本身：不走 `__APPLE__` / `_WIN32` 的平台 SDK 分支。tinyhttps 是 mcpplibs 自家库，应直接加 openkal 路径（见 §4）。 |
| "openkal 目标上 C 包可以假设什么"没写下来 | **文档缺口（非 ABI）** | 不进 SPEC 规范性条款（SPEC 讲的是 kal_ 边界，不讲 C 库），建议在 openkal README 或 `docs/consumer-profile.md` 加一段信息性说明：C 头来自 openkal-musl；三元组会定义 `__APPLE__`/`_WIN32` 但对应 SDK 不存在；网络走 musl 的 POSIX socket（底下是 `openkal.net`），不是 winsock。 |
| 包没有办法可靠判断"我在 openkal 上" | ~~T（mcpp），`__openkal__` 宏~~ → **撤回，见 §6.4** | 平台依赖由包自己在 manifest 里引入；分支在依赖解析时通过 feature 决定，不在源码里认实现。 |

### #32.3 — 交叉编译时宿主 sysroot 泄漏（mcpp#662）

**T，不是 openkal 的问题**，但优先级高：它决定了 openkal 图是不是真正封闭（hermetic）。同一个项目在有 mingw 的机器上编不过、没有 mingw 的机器上能编过。**第二部分的 index 兼容测试依赖这个修复**，否则"通过"的标签可能只是碰巧在 CI 机器上通过。修法：openkal 图里给 clang 传 `-nostdinc`（及 `-nostdinc++` 视 libc++ 来源）/ 显式 `--sysroot` 指向 store。

### #32 附带的旧项（2026-09-14/15 测量，需要按当前版本复测）

| 项 | 类别 | 结论 |
| --- | --- | --- |
| openkal-windows spawn 时 `bInheritHandles=TRUE`，继承所有可继承句柄 | **I（windows）** | 已在 `openkal-windows/src/process.cpp:243` 确认。修法：`STARTUPINFOEXW` + `PROC_THREAD_ATTRIBUTE_HANDLE_LIST`，只列出这三个流。与规范一致：`kal_spawn_streams` 只授予三个流。**这是真正的实现 bug，建议优先修。** |
| openkal-windows socket 非重叠，同一连接上阻塞读挡住写/shutdown，全双工死锁 | **I（windows）+ 规范一句话** | `src/net.cpp` 注释说明这是有意选择（为了让 `ReadFile`/`WriteFile` 同步）。§6.6 只说"顺序未指定"，没有禁止无限期阻塞，所以严格讲不违规，但对 `openkal.net` 来说连接本身就是全双工资源。**建议 §6.6 或 `net.h` 补一句**：连接上的读与写是独立的方向，一个方向等待不得阻止另一个方向或 `kal_net_shutdown`。实现改为 overlapped socket + 同步等待 `GetOverlappedResult`。 |
| -O2 下 Windows 循环被降为 `wcslen`/`strlen`、macOS syscall 包装寄存器声明 | **I（windows/macos）** | 需要复测是否已修；无 C 运行时的实现必须带 `-fno-builtin`（或提供这些符号）。 |
| openkal-linux 构造函数里忽略 SIGPIPE | **U（文档）** | 行为正确（`src/env.cpp:108` 已有说明）；在消费者文档里写一句：后台守护进程要给新的管道，否则会一直占着启动者的 stdout。 |
| musl mmap 在 Windows 上越界 | 已修（0.13.5） | 关闭 |

## 3. mcpp-index：openkal 兼容测试、标签与分类

### 3.1 现状（调研结果）

- 站点由外部的 `xpkgindex` 生成（`.github/workflows/deploy-site.yml`）。mcpp 相关的逻辑都在 `.xpkgindex/plugins/mcpp.py`。**目前只有一个 facet**：`surface`（module / header / tool / external，`mcpp.py:573`）。没有一个描述文件设置了 categories / tags。
- 描述文件 schema 里没有 tags、categories、abi 字段。`.agents/docs/2026-06-03-capability-runtime-metadata.md` 提过 `abi = "glibc"|"musl"|"msvc"`，但没有实现。
- CI (`validate.yml`) 的矩阵是 linux(gcc) + linux(llvm/libc++) + macos + windows，**没有 openkal 这条腿**，`tests/examples` 里也没有任何项目引用 openkal。
- openkal 家族 13 个包已经在 `pkgs/o/`。消费者的选法：C++ 程序依赖 `openkal-llvm-runtime`（它会带上 openkal-musl），再按平台条件依赖 `openkal-{linux,macos,windows,...}`；纯 C 程序还需要 `[build] cxx_runtime = "host-coupled"`（见 openkal-musl README）。
- 已知失败：mcpp-index#434 (libarchive / macOS / `TargetConditionals.h`)、#435 (tinyhttps / Windows / `winsock2.h`)。
- 粗分类（按包名和描述做的启发式分类，没有逐个验证）：
  - 纯计算 / header-only：约 78 个
  - 需要 OS 服务、可以经 POSIX 走 openkal：约 20 个
  - 平台 SDK / GPU / 窗口：约 53 个，这类不适用 openkal

### 3.2 "支持 openkal" 的定义：测出来，而且分级

标签只表达**测量结果**，不表达作者声明。原因和 SPEC §3.3 撤回 `hosted` 一样：一个名字如果描述的是"一类环境"，迟早会被一个没想到的环境证伪。

| 级别 | 含义 | 怎么测 |
| --- | --- | --- |
| `n/a` | 包本身就是平台 SDK / GPU / 窗口 / 宿主工具，不适用 | 排除清单，写明理由，不跑 |
| `builds` | 在 openkal 闭包里编译并链接通过 | `mcpp build`，对应 target 的 openkal 图 |
| `runs` | 该包的 `tests/examples/<lib>` 在 openkal 上运行通过 | `mcpp test` |
| `native` | 包直接 `import openkal.*`，不经过 C 库（例如 openkal-kit） | 依赖图里没有 openkal-musl |

每个级别**按 target 分别给出**：linux / macos / windows，后续再加 emscripten / uefi。站点显示成类似 `openkal: runs linux·macos, builds windows` 的形式。

另外加一个**独立的标签** `freestanding`，给 `std-freestanding*`、`*-rt`、`openarch` 用。它们是另一条轴（没有 openkal.fs 等接口），不要混进 openkal 的级别里。

### 3.3 测试怎么跑：复用现有 example，不复制项目

1. **不新建第二套测试项目。** 由脚本 `tests/openkal/overlay.lua` 从 `tests/examples/<lib>/mcpp.toml` 生成 `tests/openkal/gen/<lib>/mcpp.toml`，并加上：
   ```toml
   [dependencies]
   openkal-llvm-runtime = "<pin>"     # C++；纯 C 包改成 openkal-musl + cxx_runtime
   [target.'cfg(os = "linux")'.dependencies]
   openkal-linux = "<pin>"
   [target.'cfg(os = "macos")'.dependencies]
   openkal-macos = "<pin>"
   [target.'cfg(windows)'.dependencies]
   openkal-windows = "<pin>"
   ```
   pin 写在同一个文件 `tests/openkal/pins.toml` 里。结果必须带着这组版本，否则标签不可复现。
   长期更好的做法是 mcpp 支持类似 `mcpp test --closure openkal` 的覆盖方式，这样就不用生成 manifest。这是 mcpp 侧的需求，先用生成的方式跑起来。
2. **单独的 workflow `openkal-compat.yml`，不阻塞 PR**：每周全量跑、`workflow_dispatch` 手动跑，以及 openkal 家族包的描述文件变更时跑。普通包 PR 不受它影响。openkal 还在演进，不能让它卡住整个 index。
3. **推进顺序**：
   - 第一步：linux native，最成熟，成本最低。
   - 第二步：在 linux 上交叉编译 windows / macos，只到 `builds`，沿用 `2026-08-23-nxn-cross-build-scheme.md`。
   - 第三步：macOS / Windows runner 跑 `runs`。
4. **前置条件：mcpp#662 必须先修**（openkal 图里 clang 要 `-nostdinc` 或显式 sysroot），否则 CI 机器上装了什么会决定结果。另外加一个**反向对照**：故意在 runner 上装 mingw，确认结果不变。
5. **结果文件** `.xpkgindex/openkal-compat.json`，由 bot 提交：
   ```json
   {"pins": {"openkal": "0.12.0", "openkal-linux": "0.12.0", "...": "...", "mcpp": "2026.9.16.1"},
    "measured": "2026-09-21",
    "packages": {"compat.zstd": {"linux": "runs", "windows": "builds", "macos": "fail:#434-like"}}}
   ```
   失败项记录**第一条诊断**，以及按 §1 分类的类别 (U/T/I/S/D) 和关联 issue。
6. **排除清单** `tests/openkal/exclude.toml`：`<pkg> = "reason"`，对应上面约 53 个平台 SDK 类的包。每条都要写理由，没有理由的不能排除。

### 3.4 站点：facet + 分类页

在 `mcpp.py` 里：
- `facets()` 加第二个 facet，`key="openkal"`，值为 `native | runs | builds | n/a | untested`，取所有 target 里最好的那一级，详情页再按 target 展开。
- `on_package()` 读取 `openkal-compat.json`，写到 `pkg.extensions["mcpp"]["openkal"]`，并加一个 badge。
- 新建"openkal 生态"分类页，分三组：openkal 家族本身（spec / 实现 / musl / runtime / kit）、`native`、`runs`。页头放 pins 和测量日期。
- 描述文件 **不加** `openkal = true` 这类字段。以后如果要支持作者主动声明"不打算支持"，再讨论加一个 opt-out 字段；目前排除清单已经够用。

### 3.5 失败就是 issue 来源：分诊规则

index 跑出来的每个失败，都按 §1 分到 U/T/I/S/D。要特别预防的是：**不能因为"index 里有 N 个包编不过"就往 ABI 里加东西。** 预计的失败大类和它们的去向：

| 预计失败 | 去向 |
| --- | --- |
| 包含平台 SDK 头（#434/#435 这类） | U：包要么走平台无关路径（POSIX/kal），要么在 manifest 里自己声明平台 SDK 依赖（§6.4）。标记为 platform-bound 不算失败 |
| 事件循环依赖 epoll/kqueue（libuv、boost-asio 默认 reactor、usockets） | **D**：openkal-musl 故意不构建 epoll（它的 README 写了"readiness set 是某个内核的设施"），§11.4 也把 readiness 留在线上。使用侧能解的就解：asio 用 `BOOST_ASIO_DISABLE_EPOLL` 退到 select reactor；libuv 在 linux 上没有 poll 后端，**标 `builds`/fail，不推动 `openkal.event`**。`openkal.event` 是否定义另行决策，不由 index 驱动 |
| 信号处理器（`sigaction` 返回 ENOSYS） | D：openkal 没有异步投递 |
| `dlopen` / 插件 | D：静态闭包下不适用 |
| `mmap` 文件映射的边界情况（mimalloc、sqlite WAL、llama.cpp 加载模型） | 先按 I (musl) 查，再看是否是 S |
| 只在某个实现上失败 | I：对应的实现仓库 |

### 3.6 第一批测试名单（建议 ~25 个，覆盖面优先）

- 压缩 / 编码：zlib, zstd, lz4, xz, bzip2, brotli, xxhash
- 解析：cjson, yyjson, expat, pcre2, re2, md4c, nlohmann.json
- 工具库：fmt, spdlog（线程、文件）, CLI11, argparse
- 测试框架：catch2, doctest, gtest。**这批要最先跑通**，因为别的包的 `runs` 级别依赖它们
- 运行时压力：sqlite3（`kal_fs_lock`）, lua, mimalloc（mmap）
- 网络：mbedtls（`openkal.random`）, c-ares / curl（`openkal.net` + 用 poll 做 readiness）
- 自家：见 §4

## 4. mcpplibs 自维护库的 openkal 适配方案

| 包 | 现状判断 | 方案 | 预期级别 |
| --- | --- | --- | --- |
| **openkal-kit** | 直接 import openkal | 本来就是 `native`，放进分类页作样板 | native |
| **sbase** | 已经在 openkal-musl 上构建 | 接入兼容测试，作为 C 程序（`cxx_runtime = "host-coupled"`）的样板 | runs |
| **cmdline / cmp / xpkg / templates** | 纯计算 | 不用改代码，直接加入测试 | runs |
| **tinyhttps** (#435) | `src/socket.cppm` 用 `#ifdef _WIN32` 选 winsock | 两个 feature：`posix-socket`（默认，走 musl → `openkal.net`，因此平台无关）和 `winsock`（在 manifest 里自己声明 Windows SDK 依赖）。源码按 feature 定义的宏分支，不按 `_WIN32`，也不按任何"是不是 openkal"的宏（§6.4）。mbedtls 的熵源改走 `getrandom`，musl 底下是 `openkal.random`。它的注释里提到 SIGPIPE 的处理方式，在 openkal-linux 上本来就是忽略的，行为一致 | runs（linux）→ 三平台 |
| **llmapi** | HTTP 客户端，建在 tinyhttps 之上 | tinyhttps 适配完就能跟着过；需要真实网络的测试放到可选步骤 | runs |
| **llamacpp** | ggml CPU 后端主要是计算 + 线程 + mmap 加载模型 | 只开 CPU 后端（关闭 Metal/CUDA/Vulkan 特性），用 `kal_task_parallelism` 让 `hardware_concurrency` 返回真实值（0.10 已有）。mmap 加载先测，失败就用读文件回退（`use_mmap=false`） | builds → runs |
| **imgui** | core 是纯计算，backend 需要窗口 / GPU | **把 core 和 backend 分开**：core 的 feature 集合测 `runs`（headless 渲染出 draw list）；backend 标 `n/a` | runs（仅 core） |
| **ffmpeg** | 巨大，有大量平台分支 | 暂不适配。以后可以尝试 `--disable-everything` + 纯软件编解码的最小 feature | untested |
| **std-freestanding\* / \*-rt / openarch** | 另一条轴 | 打 `freestanding` 标签，不进 openkal 级别 | — |
| **clangtidy / grpcgen / rules-cuda** | 宿主构建工具 | `n/a` | n/a |
| **compat.libarchive** (#434) | `__APPLE__` 分支 include 了 `TargetConditionals.h` | 描述文件里的 `generated_files` 已经按 `__APPLE__` 分支；改成描述文件 feature 选择：平台无关的 POSIX 分支（默认），或者 Apple SDK 分支（自己声明 SDK 依赖）。不用空垫片，垫片是在模拟 SDK | builds → runs |

## 5. 需要 review 决定的点

1. **可执行谓词进不进 ABI**（§2 #31），以及形状：往 `kal_node_info` 尾部追加字段，还是单独加一个查询函数？这取决于你对 §5.3 "冻结"的解读。
2. **`kal_err_not_executable` 进不进**（§2 #28.3a）。这是闭合错误集自 0.5 以来的第一次增加。
3. **#28.2**：只让 openkal-linux 自己不返回 0 句柄，还是规范层面加一个 `KAL_STREAM_INHERIT` 哨兵值？我建议只改实现。
4. **§6.6 要不要补"连接的两个方向相互独立"这一句**，还是只当 openkal-windows 的 bug 处理？
5. ~~`__openkal__` 宏~~ → 撤回，见 §6.4。
6. index 兼容测试的级别 → 修订为两条轴，见 §6.5。
7. `openkal.event`（reserved）**不由 index 的失败数量驱动**。

## 6. review 后的设计结论（2026-09-17 修订）

review 补充了一条原则，它改变了上面几处结论：

> **openkal 生态里的库和应用可以依赖具体平台，由它们自己引入平台依赖。openkal 只保证：经过 `kal_*` 接口的部分与平台无关。**

推论：openkal 不是一个"必须把平台隔绝在外"的沙箱；它是**平台无关部分的最小公约面**。所以判断一个诉求进不进 ABI，要多问一句：**这个意图本身是可移植的吗？** 只在某个平台上有意义的能力，归包自己去拿平台的；多数环境都有、而且意图相同的能力，才讨论进 openkal。

### 6.1 可执行谓词：进，追加到 `kal_node_info`，加一个 setter

- **进的理由**：
  - "让解包出来的程序可以运行"是一个**可移植的意图**。凡是卷上记录可执行性的环境都有它，不是 POSIX 专属形状。
  - 在线上组合不出来。
  - 缺了它是**静默错误**。index 里的 `compat.libarchive` 在 openkal 上解压 tar 时会丢掉 x 位，没有任何报错；这是比 #31 更普遍的受害者。
  - 按 §6 的原则，一个想做平台特定事情的包本来可以自己调平台 API；但"解包并让它可运行"这件事的意图不属于任何平台。如果把它推给每个包各自去拿平台，平台无关部分就空了一块。
- **形状**：
  - 读：在 `kal_node_info` 尾部追加 `int executable;`，加 `KAL_INFO_EXECUTABLE` 位。SPEC §4.2 明确说 `kal_node_info` 是**唯一允许增长的结构**，`self_size` 就是为此存在的；`static_assert` 固定的是已有字段的偏移。§5.3 正文要补一句引用这个例外，否则两处看起来矛盾。**不另加查询函数**：`kal_fs_info` 已经有 `wanted`/`present` 机制，再加一个查询是重复的入口（§7.12 的原则是"一个保留字而不是五个操作"）。
  - 写：加 `kal_fs_set_executable_at(kal_dir base, name, len, int executable)`，对应 `KAL_FS_PROP_EXECUTABLE`（卷级属性，§6.2），不声明该属性的卷返回 `kal_err_not_supported`。setter 必须是一个操作，因为 `kal_node_info` 只能用来读。
  - 语义只有"能否作为程序启动"。POSIX 实现在设置时打开 u/g/o 的 x 位，**只针对已有读位的那些类别**（等价 `chmod +x` 遵循 umask 的通常结果），清除时清掉三个 x 位。Windows 不声明该属性：可执行性由格式/扩展名决定，**卷上不记录**；`present` 位留空，不回答"是"。
  - §11.6 加一段：可执行性不是权限。权限以身份为前提，这个谓词不以身份为前提。

### 6.2 `kal_err_not_executable`：进

- 错误值的准入标准可以明确写成：**调用者会因为这个值采取不同的动作，而且每个环境都有原生的对应值**。
  - 动作不同：`execvp` / `posix_spawnp` 收到 ENOEXEC 会改用 `/bin/sh` 重试；收到 EIO 不会。
  - 原生值：Linux/macOS 是 `ENOEXEC`，Windows 是 `ERROR_BAD_EXE_FORMAT`。三个实现各加一行映射，没有表。
  - 组合不出来：调用者自己去读文件头（`#!`、ELF、Mach-O、PE）就是在模拟内核加载器，而且是平台知识。
- 错误集"闭合"指的是**实现不得自行扩展**，不是规范不得增加。§5.2 本身就记录了 0.5 加过五个值，新增受 §8 管辖。这次按同一格式在 §5.2 的表里加一行理由即可。
- 顺带把这条准入标准写进 §5.2，以后每次加错误值都用它来审。

### 6.3 §6.6：要补规范，不只是修 openkal-windows

- **只当实现 bug 修不够**：§6.6 现在的文字（"同一句柄上并发操作的顺序未指定"）**允许**这个会死锁的实现。下一个实现者读规范，也可以合法地写出同样的死锁。
- 这不是"顺序"问题，而是**活性**问题：对端在等我写，我在等对端读，于是永远等下去。调用者在线上唯一的组合方式是 10ms 切片轮询，这本身就是 simulation（延迟 + 空转）。
- 每个环境都自然支持两个方向独立（Linux/macOS 的 socket、Windows 的 overlapped I/O），所以不需要翻译层。
- **补哪一句**：不改 §6.6 的一般规则（同一句柄的顺序仍然未指定），而是在**资源本身有两个方向**的地方写明：
  > 连接（`openkal.net`）的读与写是两个方向。一个方向上等待中的传输不得延迟另一个方向上的传输，也不得延迟 `kal_net_shutdown`。`openkal.datagram` 的发送与接收同理。
- 可以放在 `net.h` / `datagram.h`，并在 §6.6 引用一句。conformance 加一条观察：一个上下文阻塞读，另一个上下文在同一连接上写，并且必须在限定时间内完成。
- 同时修 openkal-windows：改用 overlapped socket，在 `ReadFile`/`WriteFile` 上同步等待（`GetOverlappedResult`）。

### 6.4 `__openkal__` 宏：撤回，不需要

补充的原则让这个宏变得既不需要，也有害：

- **有害**：源码按"是不是 openkal"分支，就是让源码认识实现。这违反 openkal 最核心的性质（§8 / README："换实现只改 manifest 一行，源码不改"）。它也会把"openkal"和"openkal-musl"混为一谈。openkal 的 ABI 是 `kal_*`，与用哪个 C 库无关；openkal 的头文件什么都不 include，本身不和任何平台 SDK 冲突。
- **不需要**：包真正要回答的问题不是"我在不在 openkal 上"，而是"**这一块代码用平台无关的路径，还是用平台的路径**"。这是包的**选择**，不是环境的事实，所以应该在依赖解析时决定（§6.2 的三个时间点里的第一个），而不是在编译时去探测：
  - 包提供 feature，例如 `posix-socket` 和 `winsock`，由 feature 的 `defines` 驱动源码分支。
  - 选平台路径的 feature 要**在 manifest 里自己声明平台依赖**（Windows SDK / Apple SDK 包），不能指望宿主提供。这正是 mcpp#662 必须修的原因：平台依赖必须来自依赖图，而不是来自宿主机器碰巧装了什么。
  - `_WIN32` / `__APPLE__` 由三元组定义，而且**是真的**（目标确实是 Windows / Apple），openkal 不去改它们。
- **包自己的责任**：musl 头和平台 SDK 头放在同一个翻译单元里可能冲突。选平台路径的包要把平台代码隔离到自己的 TU 或子库里。openkal 对此**不作任何保证**，也不需要保证。
- openkal 侧只需要一份信息性文档（§2 #32.2 那项保留）。内容改为："经过 `kal_*` 的部分平台无关；C 环境来自你选的 C 库（例如 openkal-musl）；平台 SDK 不在闭包里，要用就在 manifest 里自己声明。"

### 6.5 兼容测试：拆成两条轴；PR 策略改为"新包不阻塞、已有标签防回退"

**原来的四级混淆了两件事**。按补充的原则，"依赖平台"不是失败，所以要拆开：

| 轴 | 取值 | 怎么测 |
| --- | --- | --- |
| **状态**（每个 target 一个值） | `runs` / `builds` / `fails` / `n/a` / `untested` | 在 openkal 图里 build / test；构建必须是封闭的（依赖 mcpp#662） |
| **可移植性**（整个包一个值） | `portable`：除了按 target 选 `openkal-*` 实现之外，没有其他平台条件依赖<br>`platform-bound`：某些 target 上自己声明了平台依赖 | **从解析结果推出来**（`resolution.json` 里有没有平台 SDK 类的包），不靠作者声明 |

- 原来的 `native`（直接 import openkal、不经过 C 库）降为**详情页信息**。对使用者来说，它不是"支持程度更高"，只是实现方式不同。
- 站点显示成 `openkal · runs(linux,macos,windows) · portable`。分类页默认列出 `portable` 且至少有一个 `runs` 的包，`platform-bound` 的包单独分组。**它们同样属于 openkal 生态**。
- `n/a` 只留给"不可能在 openkal 图里构建"的包，例如宿主构建工具。GPU / 窗口类的包如果自己声明了平台依赖、能在 openkal 图里构建，就是 `platform-bound`，不是 `n/a`。因此排除清单会比 §3.3 估计的 53 个短得多。

**PR 策略（棘轮）**：
1. 没有标签的包：openkal 这条线**不阻塞**，只在 PR 里报告结果。
2. 已有 `runs`/`builds` 标签的包：PR 让它**降级时，这个检查变成必需项**。要么修好，要么在 PR 里显式降级并写明原因（更新结果文件）。标签一旦发布，就是对使用者的承诺，不能被一个无关 PR 悄悄抹掉。
3. openkal 家族本身升版本（pins 变化）时，跑全量；这时出现的回退**算 openkal 的问题**，由 openkal 侧处理或者不升 pin，不去阻塞各个包。
4. 启用第 2 条的前提：mcpp#662 已修，并且至少连续两周全量结果稳定（没有抖动），否则棘轮会卡住无辜的 PR。
