# 三份报告的综合方案：枚举的释放、线程栈的大小、输入的改写

第二版，2026-10-25。状态：**实施所依据的方案**。第一版的结论经实测修正三处（§4），
本版取代第一版。规范对照基于 `SPEC.md` 0.15，结果为 0.16。

| 报告 | 性质 | 落点 |
| --- | --- | --- |
| openkal-linux#34（及 PR #35） | 关闭目录不结束其未读完的枚举，描述符泄漏 | 规范条文 + 五个实现 |
| openkal-musl#48 | 线程栈大小被忽略；未用的栈被全部提交 | 规范新增操作 + 三个实现 + C 库 |
| openkal#48 | raw 模式下回车与 Ctrl+J 不可区分 | 规范新增位置 + 四个实现 + C 库 |

---

## 1. 结论

1. **`kal_task_start_sized`**：以下限表达栈大小；环境给不出时报告 `kal_err_no_memory`。
2. **`KAL_TERM_PASS_INPUT`（bit 3）**：环境不改写所输入的字节。它与 `KAL_TERM_PASS_CONTROL` 相互独立。
3. **fs.h 补述**：关闭目录即释放其迭代器，`kal_fs_list_next` 拒绝已关闭的目录。
4. 实现层另修三处报告未覆盖的缺陷：
   - Linux 线程栈下方无保护页；
   - musl 为不使用的栈映射并逐字节清零；
   - 系统调用分派函数在 -O0 下栈帧为 69,008 字节。

规范只新增声明，不改既有声明（条款 8）；旧实现对新位置读作零，所言为真（条款 6.2）。

---

## 2. 仓库、版本与依赖

| 仓库 | 版本 | 内容 |
| --- | --- | --- |
| openkal | 0.15.0 → 0.16.0 | 两项声明、fs.h 补述、条款 11 第 22–24 条、conformance 观测 |
| openkal-linux | 0.16.1 → 0.17.0 | PR #35（保留原作者）、枚举安全释放、保护页、8 MiB、sized、PASS_INPUT |
| openkal-macos | 0.13.0 → 0.14.0 | 枚举、8 MiB 与 256 KiB 保护页、sized、PASS_INPUT |
| openkal-windows | 0.11.0 → 0.12.0 | 枚举、8 MiB 预留、sized、PASS_INPUT（虚拟终端输入） |
| openkal-emscripten | 0.4.0 → 0.5.0 | 枚举、sized、PASS_INPUT |
| openkal-opensbi | 0.8.2 → 0.8.3 | 仅随规范移动版本钉 |
| openkal-musl | 0.20.1 → 0.21.0 | termios 映射、`pthread_create` 五行、`__clone` 传尺寸、分派帧 |
| openkal-llvm-runtime | 0.15.4 → 0.15.5 | 仅移动 openkal-musl 版本钉 |
| mcpp-index | — | 上述各版本的索引行（GLOBAL 与 CN） |

openkal-uefi 不提供受影响的接口，也不在 musl 的依赖图中，不动。

**任务依赖**（箭头表示"须先完成"）：

```
S  规范 0.16 ──┬─> B  五个实现（并行） ──> P  openkal-musl ──> R  openkal-llvm-runtime
               └─> B' openkal-opensbi（版本钉）─┘
R ──> T 打标签与发布（GitHub 发布 + gtc 镜像到 gitcode） ──> I mcpp-index ──> V xlings 沙箱验证
```

所有仓库使用同名分支，每个仓库的 CI 因此能取到其余仓库的同名分支，联合测试未发布的整体（沿用上一轮的约定）。每个仓库一个 PR，提交作为检查点。

---

## 3. 设计

### 3.1 目录枚举（fs）

- listing 记入一张表，记录其目录的描述符或句柄。`kal_fs_close_dir` 在关闭前结束该目录的全部 listing：先结束，因为关闭后这个编号可能已指向另一个目录。
- `kal_fs_list_next` 先校验目录句柄，已释放即 `kal_err_invalid`（条款 6.7）；再在表中**查找**迭代器，而不是直接沿它读内存。因此陈旧的迭代器，或与其目录不匹配的迭代器，都被拒绝，且不会被读穿。
- 关闭与另一上下文的 `kal_fs_list_next` 并发时，被读取中的 listing 只做标记，由读取方退出时释放（条款 6.6）。两个上下文读同一枚举时轮流进行。
- 锁：Linux、macOS、emscripten 用自旋锁（与分配器同理，不依赖 openkal.task）；Windows 用 SRW 锁。
- 观测放在各实现自己的测试里，依据条款 7.13 的先例：句柄计数不可移植。

### 3.2 栈大小（task）

- `kal_task_start_sized(entry, arg, stack_size, out)`：上下文经 `kal_task_stack` 得知的大小不小于 `stack_size`。实现可以多给，零表示不要求。
- 以下限而非确切值表达：musl 每个线程都请求其默认的 128 KiB，按确切值实现会使所有线程变小。
- 给不出即 `kal_err_no_memory`，依据条款 3.1（限定可用量是供给）。不设 props 位，也不允许 not_supported（条款 6.1）。
- 不提供以区域为参数的形式（条款 6.4）。

各实现：

| 实现 | 默认 | 保护页 | sized |
| --- | --- | --- | --- |
| Linux standalone | 8 MiB `mmap` | 256 KiB `PROT_NONE` | 自映射，`max(请求, 默认)` |
| Linux hosted | 运行时自定 | 运行时的 | `pthread_attr_setstacksize`（加余量） |
| macOS | 8 MiB | 256 KiB | standalone 自行组成系统的线程属性（名字被 musl 占用），hosted 调用系统函数；每次 CI 逐字节比对两者 |
| Windows | 8 MiB 预留 | 环境自有，且编译器逐页探测 | `CreateThread` + `STACK_SIZE_PARAM_IS_A_RESERVATION` |
| emscripten | 平台默认 | 平台的 | `pthread_attr_setstacksize`；不提高默认值，因为线性内存即时占用 |

保护页取 256 KiB，因为 musl 分派函数在 -O0 下实测有 69,008 字节的帧，一页的保护会被一帧越过。

musl：
- `pthread_create` 改五行：只映射 TLS、TSD 和启动参数，不映射栈；把请求的大小记入 `stack_size`；拒绝调用者提供的栈（`EINVAL`）。
- `__clone` 由线程指针取得 `struct pthread`，把大小传给 `kal_task_start_sized`（弱引用，缺失时退回 `kal_task_start`）。
- `stack_size` 在 musl 中别无读者；`stack` 被 `h_errno.c` 用来判断是否主线程，故保持非空。

### 3.3 输入改写（terminal）

- `KAL_TERM_PASS_INPUT`：置位表示所输入的字节原样到达，回车为 0x0d，不被改写为 0x0a。
- 它覆盖环境改写输入的全部机制，沿用 PASS_CONTROL 的规则：termios 上是 `ISTRIP|INLCR|IGNCR|ICRNL`，四者皆清才读作置位；清除该位置时只恢复 `ICRNL`；请求值与当前值相同则不写。
- 与行编辑相互独立；后果（行编辑下回车不再结束一行）写入条文。

| 实现 | 映射 |
| --- | --- |
| Linux、macOS、emscripten | 上述四个 termios 标志 |
| Windows | 行输入关闭 **且** `ENABLE_VIRTUAL_TERMINAL_INPUT` 打开时置位；请求置位时打开后者；行输入打开时读作清除，请求不改变它 |

musl：
- `tcsetattr` 在四标志皆清时请求该位置（`cfmakeraw` 即如此）。
- 在无回显的 raw 模式下，若程序保留 `ICRNL` 或 `ISTRIP`，也请求该位置，由本层改写。这一状态属于终端而非描述符，作用于交互式标准流的读取。
- `tcgetattr` 报告的是实际生效的状态。

---

## 4. 实测修正了第一版的什么

| 第一版 | 实测 | 第二版 |
| --- | --- | --- |
| Windows：行输入关闭即"不改写"，PASS_INPUT 读作置位 | 控制台关闭行输入、未开虚拟终端输入时，回车为 0x0d，Ctrl+J（三种写法的键事件）**不产生任何字节**；打开虚拟终端输入后，回车 0x0d、Ctrl+J 0x0a；行输入下为 `61 0d 0a` | PASS_INPUT ＝ 行输入关闭 ∧ 虚拟终端输入；由后者建立 |
| musl 按描述符记录"由本层改写" | 程序常经 fd 1 设置、经 fd 0 读取；且在新的 Windows 映射下，原条件在三个实现上都不会成立 | 改为终端级状态；条件改为"无回显的 raw 模式下保留 ICRNL/ISTRIP" |
| 规范 conformance 观测描述符泄漏 | CI 的 RLIMIT_NOFILE 可达 1048576，泄漏不可见 | 由各实现以自身的句柄表观测 |

---

## 5. 测量

| 断言 | 手段 | 结果 |
| --- | --- | --- |
| PR #35 的测试能检出缺陷 | Linux：新测试配旧 `fs.cpp` | 失败；配新 `fs.cpp` 通过 |
| 提前关闭不再泄漏 | 各实现自身测试：Linux `/proc/self/fd`、macOS `fcntl`、Windows `GetProcessHandleCount` | Windows 104/104，其余相等 |
| 关闭与读取竞争 200 次 | 各实现自身测试 | 每次读取方都得到报告，进程存活 |
| 至少给出所请求的栈 | 规范 conformance（4 MiB 请求，递归 3 MiB）+ 各实现测试（1/16/64 MiB） | Linux、macOS、Windows（三个工具链）均成立；Windows 1 MiB → 8.25 MiB，64 MiB → 64.25 MiB |
| musl 上 `setstacksize(4 MiB)` 有效 | `examples/stack-size`，main 对照 | main：约 256 KiB 处 SIGSEGV；本轮：成立 |
| 256 MiB 请求不再提交内存 | 同上（Linux 读 VmRSS） | 未增长 |
| 调用者提供的栈被拒绝 | 同上 | `EINVAL` |
| 栈下一字节读即故障 | 同上，在 `posix_spawn` 启动的程序中；macOS hosted 测试在 fork 子进程中 | Linux、macOS 均故障 |
| 分派函数帧 | clang 22.1.8 -O0 反汇编 | `sub rsp, 0x10d90` → `0x2e10` |
| raw 下回车与 Ctrl+J | musl `examples/terminal` 经 pty 与系统 C 库逐字节对照（`cfmakeraw` 与保留 ICRNL 两种） | 两种模式均一致：`0d 0a` 与 `0a 0a` |
| PASS_INPUT 的往返与"不重写" | Linux、macOS：自建 pty；Windows：自建控制台并写入键事件 | 成立 |
| 规范 conformance | 三平台五行 + emscripten | Linux 200 held / 0 / 3 not observed；Windows 182 / 0 / 6 |

另见：macOS 上 musl 经 `fork` 复制的程序中，`pthread_create` 返回 `EAGAIN`；未修改的 main 同样如此（§7）。

---

## 6. 多角度审查

**架构。** 两项声明都是资源的性质，由资源本身回答：栈大小以上下文为资源，输入改写以 stream 为资源。没有新增结构、错误值或注册表（条款 5.3、5.2、7.1）。规范之外的不对称由 C 库承担：它知道调用者要什么，所以在 raw 模式下自己完成改写。这一分工与上一轮 `pthread_getattr_np` 相同。

**稳定性。**
- 迭代器被查找而非信任，因此陈旧的迭代器、不匹配的目录、并发关闭都不会造成释放后使用。
- 栈有保护页，溢出即故障，而不是写入相邻映射。
- 代价：`kal_space_start` 时若表锁或 busy 标记被别的上下文持有，子进程会永远等待。这与分配器锁是同一类已记录的限制。

**简洁。** 一个操作、一个位置、一段条文补述。termios 的四个输入标志并为一个位置，不按机制拆分（第 20 条的粒度）。musl 的改动限定为五行并记入 PATCHES.md。

**用户体验。**
- 报告者的三个场景各自独立修复：轮询 `/proc` 不再耗尽描述符；`setstacksize` 生效；`cfmakeraw` 下回车为 0x0d、Ctrl+J 为 0x0a。
- 不设置栈大小的程序（`std::thread` 无法设置）在三个原生平台上都得到 8 MiB。
- Windows 上 raw 模式的方向键、功能键开始以 Unix 终端的转义序列到达。

**兼容性。**
- 只新增声明。旧实现对 PASS_INPUT 读作零，musl 据此报告 `ICRNL` 置位，为真。
- 旧实现缺 `kal_task_start_sized` 时，musl 经弱引用退回 `kal_task_start`。
- 行为变化：
  - 已关闭目录的迭代器从"继续可用"变为被拒绝（fs.h 原本就称其已释放）；
  - `pthread_attr_setstack` 从"成功但不使用"变为 `EINVAL`；
  - macOS、Windows 的默认线程栈从 512 KiB / 1 MiB 变为 8 MiB（地址空间，不是内存）。

**跨平台。** 置位时三平台字节相同，已测。默认栈三平台相同，已测。清除时的改写方式各平台不同，这是"零为较弱主张"的含义；C 库在 raw 模式下消除这种差异。

**一致性。** 五个实现按同一模式修复枚举（表、查找、busy/doomed），按同一下限语义实现 sized，PASS_INPUT 在 termios 上的四标志映射在三个实现中逐字一致。

**无感升级。** 旧程序在新实现上照常编译、照常运行。新程序在旧实现上链接 `kal_task_start_sized` 会在链接期失败（条款 6.2 表中 link 一栏）；musl 用弱引用避免把这一点传递给 C 程序。

**测试覆盖。** 每项断言都有观测，且观测的是调用者依赖的性质：可用空间而非数值，字节而非标志，句柄计数而非返回值。三平台都经 CI 实测。未覆盖：
- emscripten 的终端（node 下无 pty）；
- 真实键盘（以键事件代替；键事件即键盘驱动产生的同一记录）。

---

## 7. 待定与已知限制

1. **macOS（standalone）上 fork 后的线程启动**：复制出的程序中 `pthread_create` 返回 `EAGAIN`。在 CI 上以未修改的 main（openkal-musl 0.20.1、openkal-macos 0.13.0）复现：`parent rc=0`，`copy rc=11`。这是既有缺陷，不由本轮引入，已报告为 `mcpplibs/openkal-musl#55`。
2. **Windows 行输入模式**下回车以 `0d 0a` 结束一行，与 Unix 的 `0a` 不同。这是行编辑而非 raw 输入的差异，不在本轮范围内。
3. **`do_mmap` 对一切映射逐字节清零**（不止线程栈），根治需要 openkal.memory 声明零页属性。另行处理。
