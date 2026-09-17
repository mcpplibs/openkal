# openkal 生态跨仓库设计：边界、层、封闭性与兼容性度量

- 日期：2026-09-17
- 状态：设计稿，待 review
- 取代/汇总：
  - `openkal/.agents/docs/2026-09-17-issue-triage-and-index-openkal-compat.md`（§1–§6，本文以其 §6 修订后的结论为准）
  - `mcpp/.agents/docs/2026-09-17-issue-662-graph-target-header-isolation-plan.md`（M1–M4、I1–I3，本文对 I1 和 D3 提出修订）
- 涉及仓库：`mcpplibs/openkal`，`openkal-{linux,macos,windows}`，`openkal-musl`，`mcpp-community/mcpp`，`mcpplibs/mcpp-index`，`mcpplibs/tinyhttps`
- 事实来源：
  - SPEC 0.12
  - mcpp `docs/22-target-side.md`（五个目标侧层）
  - #662 方案的实测（mcpp 2026.9.17.2）
  - `compat.zlib.lua`、zlib 1.3.1 `zconf.h`
  - openkal-windows `src/process.cpp`、`src/net.cpp`
  - openkal-linux `src/sys.h`

---

## 0. 一句话

**每一层只保证自己那一层，并且整层由依赖图提供；包按"造成差异的那一层"适配；平台依赖允许存在，但必须来自依赖图，并且不能泄漏给别人；"支持"是测出来的，不是声明的。**

---

## 1. 边界：谁保证什么

mcpp 已经把目标侧分成五层（`docs/22-target-side.md`）。openkal 生态的全部分工，都可以落到这五层加上"包"上：

| 层（mcpp 名） | openkal 图里由谁提供 | **保证什么** | **不保证什么** |
| --- | --- | --- | --- |
| `kernel-abi = openkal` | `openkal` 规范 + `openkal-{linux,macos,windows,…}` 实现 | 经过 `kal_*` 的行为与平台无关；缺失的接口在链接时暴露；缺失的属性由 props 回答 | C 库是什么；平台 SDK 是否可用；包的其他代码是否可移植 |
| `c-abi = musl` | `openkal-musl` | 一个 POSIX 形状的 C 环境，建在 `kal_*` 之上；做不到的就显式拒绝（ENOSYS），并写进 limits 表 | Windows CRT / Apple SDK 头；epoll、信号处理器等 openkal 按设计没有的东西 |
| `c++-abi = libc++` | `openkal-llvm-runtime` | 为这个 C 库配置好的 libc++/libc++abi/libunwind | — |
| `compiler` / `compiler-runtime` | 工具链 | 编译器自带头文件（resource dir）、builtins | — |
| **（引擎）** | mcpp | **封闭性**：图提供的层，头文件和库都只来自图，宿主机器不参与 | 包的源码能不能在这个 C 库上编译 |
| **（包）** | 包作者 / index 描述文件 | 按层适配；如果使用平台能力，自己把平台依赖引入图中 | — |

**由此得出的三条硬约束**（下面各节都从这里推出）：

1. **openkal ≠ openkal-musl。** `kernel-abi` 和 `c-abi` 是两层。openkal 的头文件什么都不 include，它本身和任何平台 SDK 都不冲突；与 Windows SDK 冲突的是 musl 的头文件。所以**头文件问题要按 `c-abi` 适配，不按 `kernel-abi`**。
2. **平台依赖是合法的。** openkal 生态的库和应用可以依赖具体平台，只要依赖是自己引入的。openkal 只对经过 `kal_*` 的那部分负责。
3. **源码不认识实现。** 没有"我是不是在 openkal 上"这种宏（撤回 `__openkal__`）。选择发生在依赖解析时，表现为 manifest 里的 feature 或 cfg；源码只看 feature 带进来的宏。

---

## 2. 设计原则（跨仓库统一）

| # | 原则 | 落在哪里 | 反例（本轮已发现的） |
| --- | --- | --- | --- |
| P1 | **一层由图提供，就整层由图提供。** 编译侧和链接侧读同一个值 | mcpp M1 | #662：链接侧有 `-nostdlib`，编译侧没有关掉隐式搜索 |
| P2 | **按造成差异的那一层适配。** 头文件和 CRT 差异用 `c-abi`；只有 `kal_*` 的差异才可能涉及 `kernel-abi` | index 描述文件、包 | 用 `kernel-abi = "openkal"` 或 `__openkal__` 去判断"有没有 io.h" |
| P3 | **不对目标说谎。** `_WIN32`、`__APPLE__` 由三元组定义，而且是真的；包不应该改写平台事实，而应该改写自己的判断条件 | 包、描述文件 | zlib 方案里的 `-U_WIN32`（§5.2） |
| P4 | **公共头只有一种读法。** 影响公共头的配置，包自己的编译单元和消费者必须看到同一份 | 描述文件、mcpp | `-U_WIN32` 只作用于 zlib 单元，`zconf.h` 因此有两种读法 |
| P5 | **平台依赖来自图，并且对下游私有。** 包引入的平台 SDK 头不广播给依赖它的包 | mcpp（私有依赖/私有 include）、包 | 宿主 mingw 通过隐式搜索进入所有编译单元 |
| P6 | **规范最小。** 只收"可移植的意图 + 无法组合 + 各环境原生 + 能先问再调"；错误值只收"调用者会因它采取不同动作"的 | openkal SPEC | 完整 mode 字、第 n 号描述符、`openkal.event` 由失败数量驱动 |
| P7 | **支持是测出来的。** 标签来自封闭构建里的测量结果，带着 pins 和日期 | mcpp-index | 作者声明"支持 openkal" |
| P8 | **失败要能自我解释。** 行为变严格时，报错要说明是哪一层、为什么、有哪两条路可走 | mcpp M2 | 升级后的 `'io.h' file not found` 看起来像 mcpp 回归 |

---

## 3. mcpp：封闭性（#662 方案的定稿意见）

#662 的根因分析和 M1–M4 **整体同意**。下面是对 D1–D4 的决定，以及两处修订。

### 3.1 D1–D4

| 问题 | 决定 | 理由 |
| --- | --- | --- |
| D1 隔离参数放全局还是按包 | **全局** | P1。引擎不应该区分"这是不是 openkal 包"，这和源码里不应该有 `__openkal__` 是同一个道理；openkal 包自带的 `-nostdinc` 更强，叠加不冲突 |
| D2 `-nostdlibinc` 还是 `-nostdinc` | **`-nostdlibinc`** | 层归属：resource dir 属于 `compiler` 层，不属于 `c-abi` 层。需要更强隔离的包（openkal-musl 自己）继续自己声明 `-nostdinc` |
| D3 M2 诊断是否同一个 PR | **同一个 PR，但文案要修订**（§3.2） | P8 |
| D4 GCC | **在工具链模型里明确拒绝，现在不实现等价隔离** | 先穷举目标行；只要"图提供 C 库 + GCC"的行不存在，实现它就是没人用的代码。拒绝要给出原因，不能静默不隔离（P1）。等出现真实的目标行再实现 `-nostdinc -isystem <gcc include> -isystem <include-fixed>` |

### 3.2 修订一：M2 诊断要给出两条路，而不是一条

原文案只说"包需要按 c-abi 适配"。按 §1 的约束 2，平台依赖是合法的，所以应该给出两条路：

```
note: this target's C library is musl (openkal-musl@0.13.5, from the dependency graph);
      host headers are not searched.
      '<io.h>' is not part of this C library. Either
        - adapt the package to the C library:  [target.'cfg(c-abi = "musl")'.build] …
        - or bring the platform headers into the graph as a dependency of this package.
```

### 3.3 修订二：第二条路现在走不通，这是一个真实的引擎缺口（新增 M5，不并入 #662 PR）

`docs/22-target-side.md` 明确规定：**层谓词不能用来选择依赖**（`cfg(c-abi = "musl")` 下的 `dependencies` 会被报告并忽略），原因是层本身是从依赖图解析出来的。这条规则是对的，但它带来的后果是：

- 一个 platform-bound 的包，没办法写"在 `c-abi = musl` 的 Windows 上，依赖 Windows SDK 头文件包"。
- 用 `cfg(windows)` 写又太宽：普通的 Windows 构建里，SDK 已经由载荷提供，会重复。

**建议的形状（M5，需要单独设计评审）**：
- **feature 驱动，而不是层驱动。** 包声明一个 feature（例如 tinyhttps 的 `winsock`），在 feature 下声明平台 SDK 依赖；**由消费者或默认 feature 集合来选择**。这不会和"层从图解析"循环，因为 feature 是解析的输入，不是解析的结果。
- **平台 SDK 依赖必须是私有的**（P5）：它的 include 目录只给声明它的包自己的编译单元，不广播。mcpp 已经有 `private_include_dirs`（`docs/04-mcpp-toml.md:651`）；需要确认"依赖边上的私有"是否已经存在，没有的话这就是 M5 的主体。
- 同一个翻译单元里 musl 头和平台 SDK 头冲突，属于**包自己的责任**：把平台代码隔离到自己的 TU。引擎和 openkal 都不对此作保证。
- **M5 不阻塞 #662**：当前 index 里所有已知案例（zlib、libarchive、tinyhttps）都可以走第一条路。

---

## 4. mcpp-index：包级适配规则

### 4.1 适配规则（写进 `docs/repository-and-schema.md`）

1. 判断条件用 **`c-abi`**（P2），不用 `kernel-abi`，也不用任何"是否 openkal"的信号。
2. **不 `-U` 平台宏**（P3）。要改的是包自己的判断条件。可选手段按优先级：
   a. 包本身提供的配置宏（例如 zlib 的 `Z_HAVE_UNISTD_H`、libarchive 的 `HAVE_*`）；
   b. `generated_files` 生成配置头（已有的 `mcpp_zlib_config.h` 模式）；
   c. 最后才考虑源码补丁。
3. 影响公共头的宏必须对**包和消费者都生效**（P4）。如果 mcpp 当前只有"仅作用于包内编译单元"的 cflags，这就是一个需要确认的能力（§4.3）。
4. 每个在 openkal 图下改变了配置的包，测试成员里都要有 **ABI 判据**：由包的对象自己报告布局，和消费者一侧比较。#662 方案里 `zlibCompileFlags()` 的做法作为通用模式。

### 4.2 对 I1（compat.zlib）的修订

| 部分 | #662 原方案 | 修订 |
| --- | --- | --- |
| 公共头 `zconf.h` | 包内 `-U_WIN32`，消费者一侧不变，两种读法 | **`Z_HAVE_UNISTD_H` 作为对消费者也生效的定义**。已查 `zconf.h`：第 477 行 `#ifndef Z_HAVE_UNISTD_H` 尊重外部定义，第 483–489 行据此使 `z_off_t = off_t`。这样两侧 `z_off_t` 天然一致，不依赖"musl 的 `off_t` 恰好是 `long long`" |
| 私有部分 `gzguts.h`（`<io.h>`、`_lseeki64`、`_wopen`） | `-U_WIN32` | 如果 gzguts 只能靠 `_WIN32` 分支：短期可以**只对 zlib 自己的 TU** 做 `-U_WIN32`，这在 P3 上是个受控的例外，因为它不进入公共头，描述文件里要写明原因；长期换成 `generated_files` 补丁，把 gzguts 的条件改成 `_WIN32 && !defined(MCPP_ZLIB_POSIX_CRT)` |
| `gzopen_w`（公共声明，对象里没有定义） | 链接失败，可接受 | 同意：这是显式失败。另外在 `zconf.h` 读法统一之后，确认它是否依然可见 |
| ABI 判据 | `zlibCompileFlags()` 对比 `sizeof(z_off_t)` | 保留，并推广为 §4.1 第 4 条 |

需要确认：公共头读法统一，依赖 mcpp 能不能让一个描述文件的 `defines` **传播到消费者**。index 的 README（recastnavigation 那一行）写着 "a feature's `defines` reach only the package's own TUs"。如果不能传播，I1 只能保留 `-U_WIN32` 加 ABI 判据作为过渡，并把"公共 defines"作为 mcpp 需求记下（M6，小）。

### 4.3 需要 mcpp 确认或补充的 index 能力

| 编号 | 能力 | 用途 | 状态 |
| --- | --- | --- | --- |
| M5 | feature 下的私有平台 SDK 依赖 | platform-bound 的包走第二条路 | 需要设计 |
| M6 | 描述文件的公共 defines（传播到消费者） | P4，公共头只有一种读法 | 需要确认是否已有 |
| M7 | `mcpp test` 覆盖目标侧闭包（例如 `--closure openkal`），或等价的覆盖方式 | 兼容测试不用生成 manifest | 可以先用生成 manifest 的方式过渡 |

---

## 5. openkal：规范与实现（0.13 候选）

结论沿用分析报告 §6，这里只列动作和各仓库的分工。

### 5.1 规范（`mcpplibs/openkal`，一个 PR，0.13）

| 编号 | 变更 | 条款 |
| --- | --- | --- |
| K1 | `kal_node_info` 末尾追加 `int executable` + `KAL_INFO_EXECUTABLE`；新增 `kal_fs_set_executable_at` + `KAL_FS_PROP_EXECUTABLE` | fs.h；§5.3 补一句引用 §4.2 "唯一允许增长的结构"；§11.6 补一段"可执行性不是权限" |
| K2 | `kal_err_not_executable = 14` | types.h；§5.2 表加一行，并写明错误值的准入标准："调用者会因它采取不同动作，且各环境有原生对应值" |
| K3 | 有两个方向的资源，一个方向等待不得延迟另一个方向，也不得延迟 shutdown | net.h / datagram.h；§6.6 引用 |
| K4 | SURFACE.txt、declarations.c、conformance 各加观察：可执行位往返；启动非可执行格式返回 K2；全双工在限定时间内完成 | §9 |
| K5 | 信息性文档：openkal 目标上消费者可以假设什么（§1 的表） | README 或 `docs/consumer-profile.md`，不进规范性条款 |
| K6 | 关闭 #23、#24（附条款号）；#28.1 写进 §11 "已考虑，暂不定义"；#32 回复并拆分去向 | issues |

### 5.2 实现

| 仓库 | 变更 | 依赖 |
| --- | --- | --- |
| openkal-linux | K1（`fchmodat` 设置/清除 x 位；`statx` 读）；K2（`translate` 加 `ENOEXEC → 14`）；可选：流句柄不返回 0（#28.2） | K1、K2 |
| openkal-macos | K1、K2 同上 | K1、K2 |
| openkal-windows | K2（`ERROR_BAD_EXE_FORMAT → 14`）；K1 不声明属性；**独立 bug**：spawn 用 `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` 只继承三个流；K3：socket 改为 overlapped + 同步等待 | 句柄继承不依赖规范，**可以立即做** |
| openkal-musl | `chmod`/`fchmodat` 只接受改变 x 位的请求（其他位的请求仍然 ENOSYS，避免"报告成功但没做"）；`stat` 的 `st_mode` 填 x 位；`okm_errno` 加 `→ ENOEXEC`；README 修掉"以 127 结束"的过时说法 | K1、K2 |

musl 的 `chmod` 语义要单独 review。判据是：**请求的 mode 和当前 mode 在 x 位之外必须完全相同**，满足才执行，否则整个调用返回 ENOSYS，不做部分生效。
- 原来是 0644，请求 0755：只改变 x 位，接受。
- 原来是 0600，请求 0755：还要给 group/other 加读权限，拒绝。
- 请求 0600：要去掉读权限，拒绝。
- 另外：K1 只有一个谓词，setter 的结果固定为"x 位与 r 位同类别"（0644→0755）或"无 x 位"。请求的 x 位组合必须**正好等于**这两种结果之一，例如从 0644 请求 0744 会得到 0755，与请求不符，因此拒绝，不能"报告成功但结果不同"。写进 limits 表。

---

## 6. 兼容性度量（mcpp-index）

沿用分析报告 §6.5，这里补上它和封闭性的关系。

- **两条轴**：
  - 状态（每个 target 一个值）：`runs / builds / fails / n/a / untested`
  - 可移植性（整个包一个值）：`portable / platform-bound`，从解析结果里推出来
- **前置条件**：mcpp M1 已发布，index pin 已更新。没有 M1，测量结果取决于 runner 上装了什么，标签没有意义。
- **反向对照**（和 #662 的 M3 思路一致）：openkal 兼容测试的 Linux 腿**故意在 runner 上安装 mingw-w64**。修复之后结果不应该改变；如果改变，说明封闭性又被破坏了。
- **平台依赖的判定**：`platform-bound` 的判据是"解析结果里存在经由 feature 引入的私有平台 SDK 依赖"（依赖 M5）。M5 落地前，所有能通过的包都是 `portable`；走不通的包记为 `fails`，并在分诊时记成"需要 M5"。
- **分诊**：每个失败归到 U（包/描述文件）/ T（mcpp）/ I（实现）/ S（规范缺口）/ D（按设计拒绝）之一，并关联 issue。D 类的最大一组预计是 epoll 事件循环：不推动 `openkal.event`，在 asio 上尝试 `BOOST_ASIO_DISABLE_EPOLL`。
- **PR 策略（棘轮）**：
  - 没有标签的包：不阻塞 PR。
  - 已有标签的包在 PR 里降级：检查变为必需项。
  - pins 升级引起的回退：归 openkal 侧处理。
  - 前提：M1 已发布，并且全量结果连续两周稳定。
- **站点**：`mcpp.py` 增加 `openkal` facet 和分类页；数据来自 `.xpkgindex/openkal-compat.json`，不在描述文件里加字段。

---

## 7. 顺序与依赖

```
 ┌─ 立即（互不依赖）──────────────────────────────────────────────────────┐
 │ A1 openkal-windows：spawn 只继承三个流                                  │
 │ A2 openkal：关闭 #23 #24，回复 #32 并拆分去向（K6）                    │
 │ A3 mcpp #662：M1–M4（按 §3 定稿），发布                                 │
 └────────────────────────────────────────────────────────────────────────┘
            │ A3 发布
            ▼
 ┌─ 封闭性落地 ───────────────────────────────────────────────────────────┐
 │ B1 mcpp-index：pin 到新 mcpp；I1 zlib（按 §4.2 修订）；适配规则写进文档 │
 │ B2 mcpp-index：openkal-compat workflow，先做 Linux 原生 + mingw 对照； │
 │    站点 facet；第一批约 25 个包                                          │
 │ B3 mcpp：确认 M6（公共 defines）；M5 开始设计评审                         │
 └────────────────────────────────────────────────────────────────────────┘
            │                                   │
            ▼                                   ▼
 ┌─ 规范 0.13 ────────────────────────┐  ┌─ 生态扩展 ───────────────────────┐
 │ C1 openkal K1–K5                   │  │ D1 tinyhttps：posix-socket 默认；  │
 │ C2 linux/macos/windows 实现 + K3   │  │    winsock feature 等 M5          │
 │ C3 openkal-musl：chmod x 位 / stat │  │ D2 libarchive 评估（x 位依赖 C3）  │
 │    / ENOEXEC                       │  │ D3 交叉编译 windows/macos 的       │
 │ C4 index pins 升级 → 全量重测       │  │    builds 腿                       │
 └────────────────────────────────────┘  └───────────────────────────────────┘
            │
            ▼
   E1 棘轮启用（全量结果连续两周稳定之后）
```

---

## 8. 需要 review 决定的点

1. **§1 分层表**是否作为整个生态的正式边界？建议写进 K5 文档和 mcpp `docs/24-openkal-cross.md`，两处互相链接。
2. **D1–D4 的定稿**（§3.1），尤其是 D4 "先拒绝，不实现"。
3. **M2 文案给出两条路**（§3.2），以及 **M5（feature 驱动的私有平台依赖）** 是否作为生态级能力推进。它决定了"openkal 生态允许依赖平台"这句话在工具上是否真正可行。
4. **I1 的修订**（§4.2）：公共头用 `Z_HAVE_UNISTD_H` 统一读法，`-U_WIN32` 只作为 TU 私有的过渡手段。取决于 M6 是否已经存在。
5. **P3 "不对目标说谎"** 是否作为 index 的硬规则？（`-U_WIN32` 这类是否一律需要注明原因的例外。）
6. **musl `chmod` 的部分语义**（§5.2 ）：只接受改变 x 位的请求，其余一律 ENOSYS。

---

## 9. Review 记录（2026-09-17）

- 第 2、3、5、6 项：同意。
- 第 4 项：同意，并且**不要做过度方案**。zlib 不为此新增 mcpp 能力（M6 不立项）：
  - 如果描述文件本来就能把 define 传给消费者，就用 `Z_HAVE_UNISTD_H`；
  - 否则只对 zlib 自己的编译单元加 `-U_WIN32`，配合 ABI 判据，并在描述文件里写明原因。
- 第 1 项：讨论结论见 §10。

## 10. 分层设计评估：兼容性 / 自由度 / 跨平台

**结论：分层是对的，而且主要风险都被限制在单独一层里。** 但有三条边界原来没有写出来，需要补成规则（R1–R3）；另外有两个风险需要跟踪（R4–R5）。

### 10.1 成立的部分

| 维度 | 为什么成立 |
| --- | --- |
| 兼容性（演进） | `kal_*` 只增不改（§8），布局冻结，只有 `kal_node_info` 可以增长；层之间通过 pin 形成一个一致的栈（runtime 精确 pin musl）。一层升级不会悄悄改变另一层 |
| 自由度（程序级） | 用不用 openkal 是**按 target 的一行 manifest**：同一个程序可以在 Linux 上走 openkal，在 Windows 上走原生载荷。接口是可选的，缺失在链接时暴露 |
| 自由度（包级） | 包可以是 portable，也可以是 platform-bound（M5 落地后）；选择在依赖解析时做，源码不认识实现 |
| 跨平台（风险隔离） | 平台差异集中在 `kernel-abi` 的实现里。例如 macOS 系统调用号不稳定的风险（R4）只影响 openkal-macos，上面各层不用动 |
| 封闭性 | M1 之后，图提供的层完全来自图，构建结果不再取决于宿主机器上装了什么 |

### 10.2 需要补成规则的边界

**R1　一个镜像只有一个 C 运行时和一个 C++ 运行时。"platform-bound" 指的是平台 OS API，不是平台的 C 库。**

- 允许：Win32 / winsock / Cocoa 这类**系统库的 C 接口**，前提是跨越边界的只有句柄和值。
- 不允许：
  - 链接按 ucrt/msvcrt/libSystem/glibc 编译的**静态库**；
  - 让 CRT 拥有的对象跨越边界：`FILE*`，一侧 malloc 另一侧 free，`errno`，locale。
- 推论：只以 MSVC CRT 静态库形式分发的厂商 SDK（部分 CUDA 组件、预编译 ffmpeg 等），在 openkal 目标上是 `n/a`。这是设计的代价，不是缺陷。
- 写进：K5 的消费者文档、index 适配规则。

**R2　由平台创建的线程进入 C 库代码时，没有 C 库的线程状态。**

- 系统库自己创建的线程（Windows 线程池、COM、macOS 的 GCD/框架回调）不是 `kal_task_start` 启动的。§7.10 说明了 musl 把每个上下文的状态（errno、locale、pthread_self）放在 TLS 里；这类线程回调进 musl 代码时，这份状态不存在。
- 这是 platform-bound 包最可能遇到的**静默崩溃**，并且 R1 的规则挡不住它。
- 现在的处理：写进 openkal-musl 的 limits 表，以及 K5 文档。platform-bound 包必须把平台回调限制在调用者自己的线程上，或者在回调里不接触 C 库。
- 以后的处理：这是一个"采纳一个不是由我启动的上下文"的问题，属于 `openkal.task` 的范围。**现在不立项**；按 P6，等出现真实的消费者和测量结果再评估。

**R3　P2 需要细化：头文件差异看 `c-abi`，设施差异看 `(kernel-abi, c-abi)` 这一对。**

- musl 跑在 Linux 上（例如 Alpine）和 musl 跑在 openkal 上，头文件一样，**但提供的设施不同**：openkal 上没有 epoll，没有信号处理器，chmod 只能改 x 位。
- 所以：
  - 头文件或 CRT 差异（`io.h`、`TargetConditionals.h`）→ `cfg(c-abi = "musl")`；
  - 设施差异（事件循环后端）→ 优先用**包自己的 feature**（例如 asio 的 reactor 选择）；描述文件里需要自动选择时，用 `cfg(all(kernel-abi = "openkal", c-abi = "musl"))`。
- 这和撤回 `__openkal__` 不矛盾：manifest 里的 cfg 是依赖解析时的构建选择，不会进入源码。

### 10.3 需要跟踪的风险

**R4　openkal-macos 直接走系统调用，而 Apple 不承诺系统调用 ABI 稳定。**

- Go 在 macOS 上就是因为这个原因，从直接系统调用改成了经由 libSystem。
- 分层设计已经包含退路：SPEC §1 允许实现"建在 C 库之上"，openkal-macos 可以改为基于 libSystem，上层完全不用动。
- 代价：进程里会同时有 libSystem 和 musl，于是 R1、R2 适用于 openkal-macos 自己。
- 行动：兼容测试的 macOS 腿跟随 macOS 大版本升级跑全量；出现系统调用破坏时，再评估是否迁移到 libSystem。现在不迁移。

**R5　适配成本不对称：Linux 低，Windows 和 macOS 高。**

- 原因是 `_WIN32` / `__APPLE__` 在上游代码里同时表示三件事：内核、CRT、平台 API 头。在 openkal 上这三件事分开了：内核被 `kal_*` 隐藏，CRT 是 musl，平台 API 头不在依赖图里。
- 考虑过并否决的替代方案：
  - 给 openkal 定一个自己的三元组，不定义 `_WIN32`。否决：目标文件格式、调用约定和 SEH 仍然是 Windows 的，编译器三元组必须是真的；而且 platform-bound 包确实需要 `_WIN32`，违反 P3。
  - 全局 `-U_WIN32`。否决：这是对所有包说谎，还会破坏 platform-bound 包。
- 接受这个代价。它换来的是"所有平台上是同一个 C 环境"，而这正是 openkal 存在的理由。成本通过三件事变得可见、可控：index 的测量、`c-abi` 适配规则，以及程序在这个 target 上可以选择不走 openkal。

### 10.4 自由度的边界（汇总）

| 可以 | 不可以 |
| --- | --- |
| 每个 target 单独选择走不走 openkal | 在同一个镜像里同时使用 musl 和平台 CRT |
| 包使用平台 OS API（经由依赖图、私有，M5） | 链接按平台 CRT 编译的静态库；让 CRT 对象跨越边界 |
| 实现选择建在 syscall 或平台 C 库之上 | 源码识别具体是哪个 openkal 实现 |
| 可选接口缺失时在链接期得知 | 平台线程回调进 C 库代码（R2，目前只有文档约束） |
