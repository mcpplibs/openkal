# mcpp + openkal + openkal-llvm:通用交叉构建

> **命题**:一台**能跑 mcpp** 且**有 openkal 实现**的机器,应当能交叉构建出
> **每一个有 openkal 实现的平台**的程序 —— 而这不需要为每一对 (宿主, 目标)
> 准备一份工具链载荷。
>
> 状态:2026-08-23。工具链层已落地并实测;运行时层收敛到**一个谓词**,进行中。

---

## 0. 摘要

| | 内容 | 状态 |
|---|---|---|
| **I** | mcpp 的交叉模型从「按载荷」改为「按图」 | ✅ 已落地(`openkal-llvm` 工具链族) |
| **II** | C 库不再关心后端是谁 | ✅ openkal-musl 已验证(四个平台 + 只提供 `core` 的裸机) |
| **III** | **C++ 运行时**不再关心后端是谁 | 🟡 Linux + 裸机 + **Mach-O 已通**;PE 的 C++ 运行时已通,⚠️ 而**平台实现**卡住(§5.3.2) |
| **IV** | 链接期的格式差异由包携带 | ✅ Mach-O 已验证;⚠️ 而宿主的链接模型要**一处不剩**地让开(三条通道) |

**核心判断三条:**

1. ⭐ **openkal 的分层已经成立,而 C++ 运行时是唯一没有遵守它的那一层。**
   openkal-musl 证明了「一个 C 库,不关心底下是谁」是可实现的 —— 它在 Linux /
   macOS / Windows / 裸机上是同一份源码。libc++ / libc++abi / libunwind 不是:
   它们**按 OS 宏选实现**,于是「Mach-O 上的 musl」既不是 `__APPLE__` 那份也不是
   `__linux__` 那份。
2. ⭐ **「那个目标的实现存不存在」不该由构建工具判断。** openkal §6.1 在链接期
   回答它,并报出解析不了的那个 `kal_*`。构建工具去猜「这台机器能不能产出那个
   目标」,答错时说的是另一回事。
3. ⚠️ **这条路与 Zig 的做法相反,而相反是有理由的。** 调研见 §3。

---

## 1. 为什么这不是「给 mcpp 多加几个目标」

mcpp 原来的交叉模型是**载荷矩阵**:每一对 (宿主, 目标) 需要一份工具链载荷,
而那份载荷同时提供**编译器**和**目标侧**(头文件、C 库)。

`toolchain/registry.cppm` 的 `host_can_serve()` 逐字写着这个模型,而它的裸机分支
把判据说得最清楚:

> clang and lld are cross-compilers by construction — one binary emits every
> target it was built with — **so a freestanding target needs NO per-host cross
> payload**, unlike every hosted case above, **which needs a C library that only
> exists for some (host, target) pairs.**

⭐ **openkal 改变的正是那个前提。** C 库是**图里的一个包**,由正在跑的那个编译器
从源码建出来 —— 所以「只对某些 (宿主, 目标) 对存在」不再成立。留给编译器的只剩
代码生成,而那本来就是 clang 一个二进制全都能做的事。

⇒ 所以这不是加目标,是**换一个问题**:
从「有没有能产出这个目标的载荷」换成「clang 能不能发这个格式」。

### ⚠️ 明确不走 gcc 那条路

mcpp 今天能做的 hosted 交叉(Linux → Windows)靠的是 `x86_64-w64-mingw32-g++`
—— 一份 driver 只有一个目标的 gcc 载荷。那条路**每加一个目标就要一份新载荷**,
而且:

- 它给不出 macOS(没有这样的 gcc 载荷,也不会有);
- 它编不了 libc++ 的 std 模块(实测:openkal 栈在 `x86_64-windows-gnu` 上被目标表
  拽到 mingw-gcc,`--toolchain llvm@22.1.8` 都覆盖不掉);
- ⭐ 它**不验证 openkal 的任何东西** —— 目标侧来自 mingw 的头文件和 C 库,
  openkal 只是被链进去的一个库。

⇒ 本方案用 **openkal-llvm**:目标侧全部来自 openkal 生态,编译器只做代码生成。
这样「构建成功」这件事本身就是对 openkal 分层的一次验证。

---

## 2. ⭐⭐ 唯一没有遵守分层的那一层

openkal 的分层主张是:**上面的软件栈按 openkal 实现,不关心后端具体细节。**

逐层核对(全部实测,2026-08-23):

| 层 | 遵守了吗 | 证据 |
|---|---|---|
| **openkal 规范** | — | 56 个名字,三编译器 × 三系统 |
| **openkal-\<平台\>** | ✅ 按定义 | linux 53 / macos 50 / windows 50 / opensbi 23 / uefi 14 个 `kal_*` |
| **openkal-musl(C 库)** | ✅ | **同一份源码**跑在四个目标格式上;并且能按「底下实际有哪些接口」配置(`okm_opt.h`) |
| **openkal-llvm-runtime(C++ 运行时)** | ❌ **没有** | 见下 |
| **mcpp(构建工具)** | ✅ 已改 | `openkal-llvm` 工具链族 |

### C++ 运行时问的是「我在哪个 OS 上」

三处,全部实测:

```
libc++    __locale_dir/locale_base_api.h
          #if defined(__APPLE__)   → support/apple.h   → asprintf_l    ✗ musl 没有
          #elif _LIBCPP_MSVCRT_LIKE → support/windows.h → _locale_t     ✗ musl 没有
          #elif defined(__linux__)  → support/linux.h                   ✓

libc++abi cxa_guard_impl.h
          __APPLE__ → mach_port_t / pthread_mach_thread_np              ✗
          否则      → syscall(SYS_futex)                                ✗ 目标格式不对

libunwind AddressSpace.hpp / UnwindCursor.hpp
          __APPLE__ → _dyld_register_func_for_remove_image / Dl_info    ✗
```

⭐ **每一处的形状都一样**:它问「这是哪个操作系统」,并在每个答案上假定**那个系统
的 C 库**。这在 libc++ 正常被构建的每个地方都成立,因为 OS 和它的 C 库是一起来的。

⚠️ **在 openkal 体系里它们不是一起来的。** 一个为 Apple 目标格式构建的程序定义了
`__APPLE__` —— 那是关于**格式和 ABI** 的陈述 —— 而底下是 musl。于是 upstream 的
第一问**对着错的问题给出了正确答案**。

---

## 3. 调研:两条已有的路

### 3.1 Zig —— 相反的做法,而相反是有理由的

Zig `cc` / `c++` 从任何宿主交叉构建到 macOS / Windows / Linux,随发行版携带
clang + glibc + musl + libc++ + MinGW。这是本命题最接近的现成实现。

⭐ **而它在 Darwin 上的做法是相反的**:

> on Darwin, Zig always links libSystem which contains libc

即:**用目标平台自己的 C 库**(通过 libSystem 的 stub),而不是把一个外来 C 库
放到那个格式上。于是 libc++ 的 `__APPLE__` 分支是**对的**,不需要动。

⇒ 两种架构:

| | 目标上的 C 库 | libc++ 要不要改 | 上面的软件栈 |
|---|---|---|---|
| **Zig** | 每个平台用它自己的 | 不用 | **每个平台不同** |
| **openkal** | 一个(openkal-musl) | 要 | **每个平台相同** |

⚠️ **选 openkal 这条,理由不是「更省事」,而是它就是 openkal 的命题本身。**
如果 macOS 上用 Apple 的 libc,那么 openkal 之上的软件栈在 macOS 上和在 Linux 上
就是两份不同的东西 —— 而 openkal 存在的意义正是让它们是同一份。

⇒ 而且这条路的可行性已经被实测支持:`openkal-macos` 向系统**只借两个符号**
(`_clock_gettime_nsec_np`、`_pthread_create_from_mach_thread`),并且交叉产出的
C 程序在 macos-14 真机上启动并全部通过。**C 那一半已经证完了。**

### 3.2 libc++ 上游:正在把后端做成可插拔

调研 llvm-project 的 locale 重构(ldionne 主导,#114596 / #115176 / #115752 /
#117764 / #122531):

- 新的实现放在 `__locale_dir/support/`,**「mirrors what libc++ does for the
  threading support API」** —— 而线程后端**已经**是 `__config_site` 的开关
  (`_LIBCPP_HAS_THREAD_API_PTHREAD` 等,本包已经在用)。
- 目标写得很明确:*「making it possible to port libc++ to platforms that don't
  provide a BSD-like public API」*。
- 已经有「用 Newlib 作为 libc」的后端进去了(#167962)—— **即「哪个 C 库」正在
  成为一个合法的后端选择维度**。
- ⚠️ 也有一条 `Fix the locale base API on Linux with musl` 被 revert 过 —— 说明
  musl 这条线上游自己也还在动。

⭐ **结论:我们要的谓词与上游重构同向。** 今天要用覆盖目录表达,是因为那条链
(`#if defined(__APPLE__)`)还没被重构到;等它被重构到,这份覆盖会缩成一个
`__config_site` 开关,甚至可以作为一个 `support/openkal.h` 后端上游。

⇒ **维护成本是有界并且在缩小的**,这不是一个会长期发散的 fork。

---

## 4. 机制:一个谓词

**upstream 问「这是哪个 OS」;openkal 答「底下配置的是哪个 C 库」。**

这个答案已经在 `__config_site` 里,是**配置出来的事实**而不是猜测:

```c
#define _LIBCPP_HAS_MUSL_LIBC 1
```

⇒ 于是每一处 OS 分派前面加一问。已落地的第一处:

```c
// port/include/__locale_dir/locale_base_api.h
#if _LIBCPP_HAS_LOCALIZATION && _LIBCPP_HAS_MUSL_LIBC
#  include <__locale_dir/support/linux.h>     // musl 的后端,与目标格式无关
#else
#  include_next <__locale_dir/locale_base_api.h>
#endif
```

⚠️ **判据必须是「哪个 C 库」而不是「哪个 C 库,在 Apple 上」。** 第一版写成后者
(因为 Apple 是第一个暴露它的目标),第二个目标 Windows 立刻以同样的形状失败
(`_locale_t`)。收窄到一个平台就意味着每个平台再写一遍。

### 形态:覆盖目录,不是改 vendored 树

与 `openkal-musl/port/` 同一套安排:

- vendored 的 `llvm/` 与上游**逐字节相同**,`git diff` 对一份新 checkout 为空;
- 全部差异在 `port/include/`,靠清单里 include 顺序在前而遮蔽;
- 每个文件自己写清楚:它替换的是什么、为什么不能用 `__config_site` 表达。

⚠️ **为什么不用 `__config_site`**(先试过):libc++ 那条链是写死的
`#if defined(__APPLE__)`,没有覆盖点;唯一相关的旋钮 `_LIBCPP_HAS_LOCALIZATION 0`
是**整个去掉 `<locale>`**,不是换后端。砍掉一个能用的设施去绕过一个分派问题,
比遮蔽一个头文件更差。

---

## 5. 实现方案

### 5.1 工具链层(✅ 已落地,mcpp#486)

| # | 改动 | 为什么 |
|---|---|---|
| 1 | `Family::OpenkalLlvm` | **不是第四个编译器**,是同一份 llvm 载荷被问了不同的问题。装一个就有两个 |
| 2 | `family_serves_every_target()` | 目标覆盖 = clang 能发的每个三元组,不是载荷矩阵 |
| 3 | 载荷门放行 | ⚠️ 判断放在**依赖解析之前**,因为拒绝发生在那里;而它答得出来,因为这是项目**关于自己**的陈述。**图不被查询** —— 实现在不在,由 §6.1 在链接期回答 |
| 4 | 目标表的 pin 不覆盖它 | ⚠️ 实测:`--target x86_64-windows-gnu` 即使带 `--toolchain llvm@22.1.8` 也被拽到 mingw-gcc |
| 5 | `Triple::llvm_triple()` | mcpp 的词汇 `aarch64-macos` ≠ 编译器吃的 `arm64-apple-macos14.0` |
| 6 | `Toolchain::crossTargetFlag` | ⚠️ 不能从「targetTriple 非空」推 —— 原生构建也有 targetTriple |
| 7 | `std-compat-module` | 一个包提供 std 就要提供 std.compat,否则两份库被混用 |

**用法(实测有效):**

```toml
[toolchain]
default = "openkal-llvm@22.1.8"
```

```
Resolved openkal-llvm@22.1.8 → aarch64-macos      → …/clang++
Resolved openkal-llvm@22.1.8 → x86_64-windows-gnu → …/clang++
```

### 5.2 C 库层(✅ openkal-musl 已验证)

已完成,列在这里是因为它是 III 的先例:

- 同一份源码 → 四个目标格式;
- ⭐ **按「底下实际有哪些接口」配置**(`okm_opt.h` 那道缝):`core` 后端上不编
  `fs`/`process`/`task` 的调用;
- ⚠️ 数据模型按 (arch, os) 分档:`musl-generated/x86_64-windows`(LLP64)、
  `musl-generated/aarch64-macos`(Apple 的 `int64_t` 是 `long long`)。

### 5.3 C++ 运行时层(🟡 本方案主体)

按「一个谓词」逐处改,顺序按它们被链接顺序暴露的先后:

| # | 决策点 | upstream 的判据 | openkal 的答案 | 状态 |
|---|---|---|---|---|
| 1 | locale 后端 | `__APPLE__` / `_LIBCPP_MSVCRT_LIKE` / `__linux__` | `_LIBCPP_HAS_MUSL_LIBC` | ✅ |
| 2 | Apple SDK 头 | `__APPLE__` | 自写桩(只放**被读到**的名字) | ✅ 3 个 |
| 3 | libc++abi guard 的线程标识 | `__APPLE__` → Mach 端口 | 覆盖里补上平台名字;⚠️ 三个分支里只有第一个不合用,而它正是命中的 | ✅ |
| 4 | libunwind 段发现 | `__APPLE__` → dyld | **openkal-macos 实现 `_dyld_find_unwind_sections`**,取链接器合成的段边界 | ✅ |
| 5 | `refstring` 只读段判定 | `_dyld_get_image_header` | 零个镜像 | ✅ |
| 6 | `dladdr` / `_GNU_SOURCE` | 按 OS | **openkal 的事实,搬到包级** | ✅ |
| 7 | Mach-O 的 `thread_local` | 加载器 bootstrap 的描述符 | `-femulated-tls`(实验 A 已验证) | ✅ |
| 8 | compiler-rt builtins | 驱动链载荷的归档 | 按目标格式各建一份 | ⚠️ 见下 |
| 9 | `struct stat` 字段拼法 | `__APPLE__` → `st_mtimespec` | **openkal-musl 给别名** —— 那是 C 库的事 | ✅ |
| 10 | PE 的 C 运行时与 OS API | `_WIN32` → `_LIBCPP_MSVCRT_LIKE` / `_LIBCPP_WIN32API` | 同一个谓词,在 `__config` 覆盖里撤回 | ✅ |
| 11 | PE 的宽字符 `fopen` | 从 #10 再推出来的 `_LIBCPP_HAS_OPEN_WITH_WCHAR` | 同上;⚠️ 它是一个**值**,撤名字撤不掉它 | ✅ |
| 12 | ⚠️ **openkal-windows 要 `<windows.h>`** | — | **见 §5.3.2** | ❌ |

### ⚠️⚠️ 5.3.2 Windows 停在一条与 C++ 运行时无关的地方

撤回 #10 / #11 之后,std 模块编过了,而构建停在:

```
openkal-windows/src/win.h:35 → /usr/x86_64-w64-mingw32/include/windows.h
                             → …/xim-x-llvm/…/include/c++/v1/ctype.h
                             → __config:13 '__config_site' file not found
```

⭐ **这不是 C++ 运行时的问题,是平台实现的问题,而且是本方案第一次遇到的一类。**

`openkal-windows` 是**唯一一个需要厂商 SDK 头的平台实现**:

| 平台实现 | 它需要目标系统的什么 |
|---|---|
| openkal-linux | 无 —— 直接发系统调用 |
| openkal-macos | **两个符号**,由自写的 `port/libSystem.tbd` 声明 |
| openkal-opensbi | 无 —— `ecall` |
| **openkal-windows** | **`<windows.h>`** —— 一整套 SDK 头 |

⚠️ 而在没有 mingw 载荷的安排里,它够到了**宿主机上碰巧存在的**
`/usr/x86_64-w64-mingw32/include` —— 这是宿主污染,而且是**静默**的:它在一台
装了 mingw 的机器上会成功,在没装的机器上失败,两者都不会说出原因。

⇒ 两条路,与 macOS 的先例对照着看:

1. **自写声明**,像 `libSystem.tbd` 那样。openkal-windows 已经在自写 `.def`
   文件生成导入库,所以形态是现成的 —— 缺的是头文件那一半。
   ⭐ 这条与 openkal 的命题一致:平台实现只借它真正用到的名字。
2. **把 mingw 的头做成生态里的一个包**。省事,但它把一个厂商 SDK 引回了
   「上面的软件栈不关心后端」应当排除的位置。

**未定。** 而在定下来之前,Windows 这条在这套安排里不可用 —— 并且要注意
**它今天在一台装了 mingw 的机器上会「碰巧成功」**,那正是本仓库反复记录的假绿。

⚠️ **#8 是唯一一处清单表达不了的**:正确的判据是「驱动的运行时选择被替换过」,
而 `cfg()` 只能条件在目标上。搬成包级会让**宿主**链接重复定义 `__muloti4`,
因为原生链接里驱动仍然链载荷自己的归档。今天按目标格式各写一份。

### ⭐⭐ 5.3.1 达成(2026-08-23):Linux 上产出 macOS 可执行程序

```
$ mcpp build --target aarch64-macos          # 在 Linux 上

Mach-O 64-bit arm64 executable
/usr/lib/libSystem.B.dylib          ← 唯一依赖
_clock_gettime_nsec_np              ← 借的名字,仍然是那两个
_pthread_create_from_mach_thread
dyld_stub_binder                    ← 第三个属于格式,不属于本包
```

带着 `import std`、容器、算法、异常与展开期析构 —— **没有一个字节来自 Apple 的
SDK**,目标侧全部来自 openkal 生态。

⭐ 这是本方案第一次把命题验到「一个非 native 的 hosted 目标」上。此前
openkal-musl 只验到了 **C**(§3.1 的真机启动是一个 C 程序);C++ 运行时这一层
是这一轮才第一次走通。

### 5.4 链接层(🟡 已有形状,未逐格式验证)

每个目标格式的链接差异**已经**由 openkal-musl 的清单携带:

```toml
[target.'cfg(os = "linux")'.build]  ldflags = ["-nostdlib", "-static", …]
[target.'cfg(os = "macos")'.build]  ldflags = ["-nostdlib", "-lSystem", "-Wl,-e,_okm_start", …]
[target.'cfg(windows)'.build]       ldflags = ["-nostdlib", "-static", "-Wl,-e,okw_start", …]
```

⭐ **这正是本方案的形状**:目标的事实由**知道那个目标的包**携带,而不是由构建
工具的表格携带。clang driver 拿到 `--target=` 之后会自己选对 lld 的形态
(`ld.lld` / `ld64.lld` / `lld-link`)。

#### ⚠️⚠️ 已验证,而且比预想的多一件:宿主的链接模型要**一处不剩**地让开

macOS 的 `--ld-path=/usr/bin/ld` 已去掉 —— 它的注释早写了「交叉不需要这个系统的
链接器」,而 mcpp 的条件在**目标**上,`cfg(os = "macos")` 在「**在** macOS 上建」
和「**为** macOS 建」时都为真。第二种情况下那是 Linux 的 GNU ld:

```
/usr/bin/ld: Error: unable to disambiguate: -dead_strip
```

⭐ 而真正的发现是:宿主那套 C 运行时是从**三条**通道到链接线上的,
而堵住一条会看到**一模一样的报错**:

| 通道 | 是什么 |
|---|---|
| `link_toolchain_flags` | `lm.link_flags()` + 驱动的 C++ 运行时选择 |
| `payload_ld` | 同一组,第二次 |
| `atomic_ld` | 宿主的 libatomic + GNU ld 的 `push-state` 拼法 |

⚠️ **只堵第一条的人会合理地得出「这条修复没生效」的结论。** 这是本轮第二次
出现同一种误导(前一次是路径依赖的指纹缺口)。

---

## 6. 判据 —— 怎么知道做完了

⭐ **判据必须是「同一份源码 + 不同 `--target` = 同样的输出」,而不是「都能构建」。**

已有的形态:`openkal-llvm-runtime/examples/same-source` 一个目录、一个
`src/main.cpp`,CI `diff` 两个目标的输出行。今天覆盖两个目标:

```
mcpp run                             这台机器,openkal-linux 之上   ✅
mcpp run --target riscv64-none-elf   riscv64,OpenSBI 之上,无 OS    ✅
```

**做完的定义:同一个目录再加两行,四个目标输出相同。**

```
mcpp run --target aarch64-macos          → 传到 macos-14,ad hoc 签名,运行
mcpp run --target x86_64-windows-gnu     → 传到 windows-2022,运行
```

⚠️ **只构建不运行不算。** 本仓库已经踩过:「发布并核验了资产 ≠ 使用者能用上它」。
macOS 那条尤其 —— 链接成功的 Mach-O 仍可能被内核拒绝启动(要签名)。

### 反假绿

| 要验证的 | ❌ 不能用的判据 | ✅ 必须用的判据 |
|---|---|---|
| 交叉产物是那个格式的 | 构建成功 | `llvm-readobj` 报出格式,并在真机启动 |
| 用的是 openkal 的目标侧 | 链接成功 | ⭐ 命令行上**没有**宿主 sysroot;`-nostdinc` 在场 |
| C++ 运行时是本包的 | `import std` 编过了 | ⭐ 在**没有系统 libc++ 的目标**上跑通(裸机已做) |
| 谓词是「C 库」不是「平台」 | 一个平台过了 | ⭐ **两个不同目标格式**都过 |

---

## 7. 风险与未知

| # | 风险 | 评估 |
|---|---|---|
| R1 | libc++ 的 OS 分派点比已知的 6 处多 | 中。已数过 include 层(5 处 / 4 个头),但**实现层**要靠逐目标构建暴露 —— 这正是 §6 判据存在的理由 |
| R2 | 上游重构会挪动被覆盖的文件 | 低~中。⭐ 调研表明方向一致(§3.2),覆盖会缩小而不是发散;⚠️ 但每次同步 vendored 树都要重核对 |
| R3 | libc++abi guard 走 openkal.task 后,只提供 `core` 的后端上编不过 | 中。与 `okm_opt.h` 同一形状,解法已知(缝 + 单线程回落) |
| R4 | Mach-O / PE 上 DWARF 展开的实际可用性 | **未知**。Mach-O 有 `__eh_frame`,但 Apple 的工具链默认用 compact unwind;要实测 |
| R5 | macOS 交叉产物需要签名才能启动 | 已知且已解。CI 已有 `cross-macos-run` job 做 ad hoc 签名 |
| R6 | Windows 的 `_locale_t` 之外还有多少 MSVCRT 假设 | 未知。与 R1 同一性质 |

---

## 8. 与既有文档的关系

- `2026-08-22-ecosystem-closure-design.md` §9.1 定了**验收目标**(制品可移植),
  本文是它第 1 与第 3 条的实现方案。
- 本文**不涉及** §9.1.3 的「装载」那一半(一个二进制跑遍所有 OS)。那是另一件事:
  这里做的是「**同一份源码**建给不同实现」,不是「**同一个二进制**在不同 OS 上跑」。
