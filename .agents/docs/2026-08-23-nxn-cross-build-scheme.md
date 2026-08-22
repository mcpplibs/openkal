# 任意平台 → 任意平台:mcpp / openkal / openkal-llvm 的完整方案

> **命题**:一台能跑 mcpp 的机器,应当能构建出**任何已实现 openkal 的平台**上的
> 程序 —— 而一个程序能不能在某平台上跑,只取决于**它用到的 openkal 能力是不是
> 该平台实现的子集**。
>
> 2026-08-23。⚠️ 本文是完整方案;`2026-08-23-universal-cross-build-design.md` 是
> 它的第一版,已被本文取代;`2026-08-22-ecosystem-closure-design.md` 是上游背景。

---

## 0. 总判断

**N×N 不需要 N² 份工具链,因为其中两个维度是常数。**

一次交叉构建需要三样东西,传统上每样都按 (宿主, 目标) 配对提供:

| | 传统 | openkal 体系 | 维度 |
|---|---|---|---|
| **代码生成** | 每对一份编译器 | **clang 一个二进制发所有目标** | 常数 |
| **目标侧**(头/C 库/C++ 运行时) | 每对一份载荷 | **图里的包,从源码建** | 常数 |
| **链接** | 每对一份链接器 | **lld 一个二进制发所有格式** | 常数 |
| **平台实现** | — | openkal-\<平台\>,每平台一个 | **N** |

⇒ **N×N 塌成 N + 1**:N 个平台实现,一套工具链。而那个 1 已经在每台机器上了。

### 三条核心判断

1. ⭐ **ABI 由 openkal 保证,上层不使用任何后端的东西 —— 包括类型。**
   `openkal/types.h` 的类型全部由**编译器**推导,所以「`sizeof(long)` 是几」
   在 openkal 接口上**不存在**。目标 ABI 的差异只落在两个地方,而两个都不在
   openkal 之上:C 库重建 POSIX 时(POSIX 自己命名了 `long`),和编译器发码时。
   实测证明纪律有效:是 `kal_u64` 对、musl 的 `uint64_t` 错(§1.3)。
2. ⭐ **上层不需要为 openkal 重写,只需要别走错分支。** libc++ 的 POSIX 分支
   本来就已经在 openkal 上(经由 musl)。19 处 `<windows.h>` 里 12 处是让谓词
   答对就自己走对(§3.1)。
3. ⭐ **「这个程序能不能在那个平台上跑」是一次链接。** 不是运行期探测,不是清单
   声明 —— openkal §6.1 让缺失的接口在链接期报出名字(§6.1)。

---

## 1. 分层:谁回答什么

| 层 | 它回答的问题 | 随目标变吗 | 谁提供 |
|---|---|---|---|
| **应用** | — | ❌ **一份源码** | 用户 |
| **C++ 运行时** | 标准库 | ❌ 一份源码,**按 C 库配置** | openkal-llvm-runtime |
| **C 库** | POSIX 面 | ❌ 一份源码,**按目标 ABI 配置** | openkal-musl |
| **openkal 接口** | 48 + 4 个函数 | ❌ **不变**(类型由编译器推导) | openkal(规范) |
| **平台实现** | 怎么满足那 48 个 | ✅ 每平台一个 | openkal-\<平台\> |
| **目标 ABI** | `sizeof(long)`、类型拼法、调用约定、格式 | ✅ **目标自己定义** | 编译器 |
| **代码生成 / 链接** | 发什么 | ✅ 由 `--target=` 说出 | clang / lld |

⭐ **只有两层随目标变,而它们都不需要「每对一份」**:平台实现是 N 个包,
代码生成是一个二进制。

### 1.1 为什么 openkal 的接口能不变

`openkal/types.h` 的类型**全部由编译器推导**(`__UINTPTR_TYPE__` 及其同类),
不来自任何 C 库的头。

⇒ 后果是决定性的:**平台实现内部用什么头文件都跨不过那道边界**。
openkal-windows 调 Win32、openkal-linux 发系统调用、openkal-opensbi 发 `ecall`
—— 它们编译时看见的世界完全不同,而 `kal_*` 的签名逐字节一致。

**这就是 musl 能坐在 ELF / Mach-O / PE / 裸机四种格式上的原因。**

### 1.2 平台实现怎么做,openkal 不管

| 实现 | 它调什么 | 声明从哪来 |
|---|---|---|
| openkal-linux | 系统调用 | 自写调用号 |
| openkal-opensbi | SBI `ecall` | 自写扩展号 |
| openkal-macos | 两个名字 | 自写 `port/libSystem.tbd` |
| openkal-windows | Win32 API | 自写 `src/win32.h`(47 函数,248 行) |

⚠️ **四种做法都正当。** 唯一的规矩是**声明要属于这个包**,不能从厂商 SDK 现找
—— 那意味着不同机器上找到不同的东西,而**装了别的 SDK 的机器上可能悄悄成功于
不同的声明**。

⚠️ 清单怎么来:**不读源码,把头拿掉问编译器**。openkal-macos 的 tbd(2 个名字)
和 openkal-windows 的 win32.h(47 个)都是这么数出来的。

### 1.3 ⭐⭐ ABI 由 openkal 保证:上层不使用任何后端的东西,包括类型

**这是本方案的地基,而不是一条注意事项。**

`openkal/types.h` 的每一个类型都**由编译器推导**,不来自任何 C 库的头:

```c
#if defined(__UINTPTR_TYPE__)
typedef __UINTPTR_TYPE__ kal_uintptr;
```

它自己写着为什么:

> The width of a machine word, obtained from the **compiler** rather than from a
> header … Taking it from that compiler's own header instead would give this
> file an include, and **the consumer this file exists for has none.**

⇒ **规则**:openkal 之上的软件栈**不直接使用任何具体后端的东西 —— 包括类型**。
要一个 64 位无符号数就写 `kal_u64`,不写 `unsigned long`、不写 `uint64_t`、
不写 `size_t`。

⇒ **后果**:「`sizeof(long)` 是 4 还是 8」这个问题**在 openkal 接口上不存在**。
它不是被屏蔽了,是**根本没有被问出来的机会**。这就是同一个 `kal_*` 签名能在
ELF / Mach-O / PE / 裸机四种格式上逐字节一致的原因。

#### ⭐⭐ 实测:类型纪律抓出了 C 库的错,而不是相反

2026-08-23,从 Linux 交叉构建到 `arm64-apple-macos`:

```
okm_syscall.c:439: incompatible pointer types passing 'uint64_t *'
  (aka 'unsigned long *') to parameter of type 'kal_u64 *'
  (aka 'unsigned long long *')
```

两个 64 位类型,**同样的宽度,不同的类型,不能转换**。而哪一边是对的:

| | 来自 | 对不对 |
|---|---|---|
| `kal_u64` = `unsigned long long` | **编译器**(`__UINT64_TYPE__`) | ✅ Apple 的 ABI 就是这样 |
| `uint64_t` = `unsigned long` | musl 的 `bits/alltypes.h` | ❌ 那是 Linux 的答案 |

⭐ **是 openkal 的类型对,C 库的错。** 修的是 musl(新增
`musl-generated/aarch64-macos`,差一行),不是 openkal。

⇒ 这条实测同时说明两件事:
1. **类型纪律有效** —— openkal 的类型跨过目标格式仍然正确,而不带纪律的那一侧
   悄悄错了,靠编译器才发现。
2. **C 库是唯一必须知道目标 ABI 的那一层** —— 因为它重建的是 POSIX,而
   **POSIX 自己命名了 `long`**。`musl-generated/` 按 (arch, os) 分档正是为此:
   `x86_64-windows`(LLP64)、`aarch64-macos`(`int64_t` 拼作 `long long`)。

#### 判据

> **上层的每一个跨越 openkal 边界的类型,都必须是 `kal_*`。**
> 出现 `long` / `uint64_t` / `size_t` 的地方,要么它在 C 库里(合法,那层重建
> POSIX),要么它是一处应当修掉的后端泄漏。

#### ✅ 实测:当前 0 处泄漏

2026-08-23 核验(`include/openkal/*.h`,10 个头 618 行,`SURFACE.txt` 56 个名字):

```
$ grep -oE '\b(unsigned |signed )?(long long|long|short|size_t|uint[0-9]+_t|
    int[0-9]+_t|intptr_t|uintptr_t|ptrdiff_t|ssize_t|float|double)\b' \
    include/openkal/*.h | sort | uniq -c
（无输出）
```

⚠️ **空输出要先证明不是假绿** —— 阳性对照:同一批文件里 `kal_u64` 命中 8 处,
`types.h` 里 `__UINT64_TYPE__` 命中 2 处。⇒ grep 确实读到了文件。

⇒ **整个 openkal 接口上没有任何一个裸类型。** 唯一出现 `unsigned int` 的地方是
`types.h` 里 typedef 的右值本身,而且还在 `_MSC_VER` / 32 位的兜底分支
—— 首选分支是 `__UINTPTR_TYPE__`,连兜底都不需要走。

⚠️ 但这是**今天手跑的一次**,不是守卫。把它加进 conformance(每次 CI 都跑)
是 P5 的一项 —— 否则下一个新接口引入 `size_t` 时,没有任何东西会说话。

#### ⚠️ 那什么才是真正不该屏蔽的

分层仍然存在,只是边界比我先前写的更靠外:

| | 谁的事 |
|---|---|
| **openkal 接口上的类型** | ✅ openkal 保证,由编译器推导 |
| **openkal 之上的软件栈** | ✅ 只用 `kal_*`,不碰后端的任何东西 |
| **C 库重建 POSIX 时的 `long`** | ⚠️ 目标 ABI,C 库必须按 (arch, os) 配置 |
| **对象格式、调用约定、指令集** | ⚠️ 编译器由 `--target=` 如实遵守 |

⇒ 最后两行不是「openkal 屏蔽不了」,是**它们根本不在 openkal 之上** ——
一个在 C 库里,一个在编译器里。上层两行才是 openkal 的辖区,而它保证得住。

## 2. 三个被问错的问题

本方案遇到的**每一个**障碍都能归进这三条。全部实测。

### 2.1 「哪个 OS」→ 应当问「哪个 C 库」

**在**:libc++ / libc++abi / libunwind。
**为什么错**:OS 和它的 C 库在别处总是一起来的,在这里不是 —— `__APPLE__` 是
关于**格式和 ABI** 的陈述,而底下是 musl。

```
bsd_like.h:203   no member named 'asprintf_l'        ← Apple 的 locale 扩展
windows.h        no type named '_locale_t'           ← 微软的 CRT
cxa_guard_impl.h use of undeclared 'mach_port_t'     ← Mach
unwind.h → corecrt.h  typedef redefinition           ← SEH
```

**答案**:`_LIBCPP_HAS_MUSL_LIBC` —— 由本包的 `__config_site` 生成,是**配置出来
的事实**而不是猜测。

⚠️ **判据必须是「哪个 C 库」,不是「哪个 C 库,在 Apple 上」。** 第一版写成后者,
第二个目标立刻以同样形状失败。收窄到一个平台就意味着每个平台再写一遍。

### 2.2 「有没有载荷」→ 应当问「clang 能不能发这个格式」

**在**:mcpp 的 `host_can_serve()` 与目标表的 toolchain pin。

`registry.cppm` 自己写着判据:

> clang and lld are cross-compilers by construction … so a freestanding target
> needs **NO per-host cross payload**, unlike every hosted case above, **which
> needs a C library that only exists for some (host, target) pairs**.

⭐ openkal 改变的正是**那个前提** —— C 库是图里的包。

**答案**:`Family::OpenkalLlvm`(§4.1)。

### 2.3 「目标是什么」→ 必须说出来

**在**:编译线和链接线。

每一个 mcpp 能做的 hosted 交叉,都由一个 **driver 只有一个目标**的载荷伺候
(`x86_64-w64-mingw32-g++` 不需要 `--target`,因为它没得选)。所以
freestanding 之外没有任何地方发过 `--target`。

```
编译时缺 → okm_float_assert.c: LDBL_DIG '33 == 18'   ← 一条命令里两台机器
链接时缺 → ld.lld: obj/main.o: unknown file type     ← ELF 链接器拿到 Mach-O
```

⚠️ 而**宿主的目标侧要一处不剩地让开**,它从**三条**通道到链接线上,
堵一条会看到一模一样的报错(§4.2)。

---

## 3. 机制清单

### 3.1 mcpp 侧

| # | 机制 | 作用 |
|---|---|---|
| 1 | `Family::OpenkalLlvm` | **不是第四个编译器**,是同一份 llvm 载荷被问了不同的问题;目标覆盖 = clang 能发的每个三元组 |
| 2 | `family_serves_every_target()` | 载荷门放行。⚠️ **不查图** —— 实现在不在由 §6.1 在链接期答,那比「这台机器产不出」准确 |
| 3 | 目标表 pin 不覆盖它 | 否则 windows-gnu 一行钉死 gcc,`--toolchain` 也覆盖不掉 |
| 4 | `Triple::llvm_triple()` | mcpp 的词汇 ≠ 编译器吃的拼写 |
| 5 | `Toolchain::crossTargetFlag` | 在同时知道请求和编译器的地方定下,编译线 + 链接线都用 |
| 6 | 目标侧替换(三通道) | `link_toolchain_flags` / `payload_ld` / `atomic_ld` + 编译侧的 `host_compile_tokens` |
| 7 | capability 决定 flag | `hosted-standard-library` ⇒ 异常/RTTI/`-ffreestanding`/展开表 |
| 8 | `std-module` / `std-compat-module` | 一个包带自己的 std 模块,**两个一起带** |

**项目侧一句话:**

```toml
[toolchain]
default = "openkal-llvm@22.1.8"
```

### 3.2 运行时侧(openkal-llvm-runtime)

两种形态,**有偏好**:

| | 放哪 | 漂移面 |
|---|---|---|
| **头文件**里的平台分派 | `port/include/` 覆盖 + `include_next` | **零** |
| **源码**里的平台分派 | 原地标记 + `llvm/PATCHES.md` 记录 | 被标记的那几十行 |

⚠️ **能用覆盖就别用补丁。**

已落地:`__config`(撤回 `_LIBCPP_MSVCRT_LIKE`/`WIN32API`/`HAS_OPEN_WITH_WCHAR`)、
`__locale_dir/locale_base_api.h`、`__thread/support.h`、`__atomic/contention_t.h`、
`__atomic/atomic_waitable_traits.h`、`mach-o/dyld.h`、`AvailabilityMacros.h`;
`llvm/libcxx/src/atomic.cpp`(标记补丁)。

### 3.3 平台侧

每个 openkal-\<平台\> 自写它调的那套声明(§1.2),并通过 `build.mcpp` 把**目标侧
输入**放上消费者的链接线(openkal-macos 的 `link_search("port")`)。

### 3.4 ⭐ 移植规矩(与 openkal-musl 一致,记在 `llvm/PATCHES.md`)

1. **可以动源码。** 移植就是动源码;假装不动只会把差异藏进别处。
2. **动过的地方标注清楚。** `// ─── openkal ─── BEGIN/END`,`grep` 一次数完。
3. **能不动的就不动。** 先问「上游有没有一条路已经通向 openkal」。

---

## 4. 两条反复出现的陷阱

### 4.1 一个事实走两条通道 —— 堵一条,报错一字不变

出现了**三次**,每次都让人合理地得出「这条修复没生效」:

| 场次 | 通道 A | 通道 B |
|---|---|---|
| 链接线的宿主 C 运行时 | `link_toolchain_flags` | `payload_ld` + `atomic_ld` |
| `std::atomic` 的等待宽度 | `__cxx_contention_t` | `_LIBCPP_NATIVE_PLATFORM_WAIT_SIZES` |
| mcpp 的依赖指纹 | 根的编译输入 | ⚠️ 依赖的 **从来没进过** |

⇒ **教训:改完看到同样的报错,先假设有第二条通道,不要假设修复无效。**

### 4.2 「碰巧成功」比失败更糟

- 厂商 SDK 从宿主机找到 ⇒ 装了的机器上成功,没装的失败,**装了别版本的悄悄用了
  不同的声明**
- 本机绝对路径进清单 ⇒ 别的机器 CI 挂在一条只存在于一台笔记本上的路径
- 路径依赖的清单变更不进指纹 ⇒ **改了像没改**

⇒ 所有目标侧输入必须来自**被解析的包**。

---

## 5. N×N 矩阵与现状

**宿主 × 目标**,✅ = 实测跑通,🟢 = 实测产出,🔵 = CI 中(本轮新加):

| 宿主 \ 目标 | Linux | macOS | Windows | 裸机 riscv64 |
|---|---|---|---|---|
| **Linux** | ✅ 跑通 | 🟢 Mach-O;🔵 真机运行 | ✅ 跑通(wine);🔵 真机运行 | ✅ 跑通(QEMU) |
| **macOS** | 🔵 | ✅ 原生(🔵) | 🔵 | 🔵 |
| **Windows** | 🔵 | 🔵 | ✅ 原生(🔵) | 🔵 |

⚠️ **矩阵的形状本身是结论**:宿主那一维**不该有内容** —— 一旦目标侧全部来自包,
「在哪台机器上建」就不再是一个变量。

⭐⭐ **而这一轮证明了那不是自动成立的。** 写宿主维度的 CI 时,顺着「改完主动找
第二条通道」的规矩去读 mcpp 的链接行拼装,发现它分三支:

```c++
if constexpr (is_windows)                 { … }   // 这台机器是 Windows
else if constexpr (needs_explicit_libcxx) { … }   // 这台机器是 macOS
else                                      { … }   // 这台机器是 Linux
```

**只有第三支消费 `link_toolchain_flags`**,而 `--target=` 就在里面。⇒ 从 macOS
或 Windows 宿主交叉构建会把产物递给一个没被告知目标的链接器。修法是整条替换
(与 freestanding 那块同一个先例),并且**去掉了一个特例而不是新增一个**。

⇒ 结论仍然是「宿主那一维不该有内容」,但它是**被做出来的**,不是自然成立的。

### ⭐ 顺带修好的一处宿主泄漏

macOS 产物的链接行上原本有

```
-L…/xim-x-llvm/22.1.8/lib/x86_64-unknown-linux-gnu
-Wl,-rpath,…/lib/x86_64-unknown-linux-gnu
```

`ld64` 接受 `-rpath` 并把它写进镜像 —— 一个指向 Linux 目录的 `LC_RPATH`。整条
替换之后 `LC_RPATH` 为空。⚠️ 这条**不会让任何构建失败**,所以只可能被读出来。

### Windows 目标:从「停在 LLP64」到跑通

本轮解决,按被发现的顺序(每一条都是修好上一条才露出来的):

| # | 症状 | 真因 | 归谁 |
|---|---|---|---|
| 1 | 四目标只成两个,报错不提工具链族 | 例子清单写的是 `llvm@` 不是 `openkal-llvm@` | 例子 |
| 2 | `LONG_MAX … cannot be narrowed to 'long'` | `musl-generated/x86_64-windows` 的 `__LONG_MAX` 还是 LP64 的值 —— **同一份文件里数据模型说了两遍,只改了一遍** | C 库 |
| 3 | `__mingw_aligned_malloc` 未声明 | `AddressSpace.hpp` 的 `<windows.h>` 拉进**宿主的 mingw sysroot** | 移植 |
| 4 | 同上,挪到下一个文件 | `UnwindCursor.hpp` 一处纯粹没被用到的 include | 移植 |
| 5 | `obj/*.o: unknown file type` × 30 | 链接行没有 `--target=` —— mingw 分支提前 return | mcpp |
| 6 | `duplicate symbol: .weak._Znwy…` × 30 | `operator new/delete` 定义了两遍;**ELF/Mach-O 上弱符号静默去重,COFF 不会** | 包 |
| 7 | `__gxx_personality_seh0` 未定义,引用自**使用者的** `main.o` | `-fdwarf-exceptions` 写在包里,只覆盖了包自己的对象 | mcpp |
| 8 | `__libcpp_mutex_lock(void**)` 未定义 | `__config_site` 四个线程 API 全写 0 ⇒ libc++ 按 OS 自选,PE 上选了 Win32 | 配置 |
| 9 | `_tls_index` 未定义 | PE 的 TLS 要动态加载器 bootstrap —— **与 Mach-O 的 `_tlv_bootstrap` 同一堵墙** | 包 + mcpp |

⭐ 第 3、4 两条之后,`libunwind` 里问「这个镜像的展开表在哪」的那一支改成了
**镜像自读**(`__ImageBase` + 段表),因为这份文件已经用三种不问 OS 的方式答过
这个问题(裸机读链接器符号、Darwin 读 `_dyld_find_unwind_sections`、ELF 读自己的
program headers)。

⚠️ 走过一条错路并记进 `PATCHES.md`:先试 COFF 分组段夹住 `.eh_frame`,实测链接器
把不带 `$` 的段排在最前 ⇒ 区间为空。**那不是链接失败,是静默的错答案。**

---

## 6. 判据

### 6.1 「能不能跑在那个平台上」= 一次链接

openkal §6.1:实现不提供的接口,作为链接期定义是缺席的,用了它的消费者链接失败。

⇒ **不需要新机制。** §3.3 的命名束(`core` / `hosted`)是把这句话说成人话的
词汇:「这个程序要 `hosted`」对上「这块板给 `core`」。

⚠️ 两个方向都实测过:裸机 `import std` 首链 15 个 `kal_*` 未定义(要 `hosted`,
给 `core`);C 库按目标配置后 0 个。以及本轮 `std::atomic` 走 openkal 之后,
裸机链接停在 `kal_task_wait` —— **同一条判据,第三次工作**。

### 6.2 交付判据:同一份源码 + 不同 `--target` = **同样的输出**

`openkal-llvm-runtime/examples/same-source` 一个目录、一个 `src/main.cpp`。
CI **`diff` 两个目标的输出行** —— 断言的是「输出相同」而不是「两边都有输出」。

**做完的定义:四个目标,输出相同,且都在真机/模拟器上跑过。**

### 6.3 反假绿

| 要验证的 | ❌ 不能用 | ✅ 必须用 |
|---|---|---|
| 产物是那个格式 | 构建成功 | `llvm-readobj` + **真机启动** |
| 用的是 openkal 的目标侧 | 链接成功 | ⭐ 命令行上**没有**宿主 sysroot |
| C++ 运行时是本包的 | `import std` 编过 | ⭐ 在**没有系统 libc++ 的目标**上跑通 |
| 谓词是「C 库」不是「平台」 | 一个平台过 | ⭐ **两个不同目标格式**都过 |
| 声明属于本包 | 编译通过 | ⭐ 在**没装那个 SDK 的机器**上编译通过 |

---

## 7. 执行计划

| 阶段 | 内容 | 状态 |
|---|---|---|
| **P0** | 工具链族 + `--target` 两条线 + 目标侧替换 | ✅ |
| **P1** | 谓词从「哪个 OS」改为「哪个 C 库」(libc++ 17 处) | ✅ |
| **P2** | macOS 目标打通(Mach-O 产出) | ✅ |
| **P3** | 移植规矩 + `PATCHES.md` + `std::atomic` 走 openkal | ✅ |
| **P4** | Windows 目标打通(LLP64、三处 `<windows.h>`、emutls、链接行 `--target`) | ✅ **PE 产出并跑通** |
| **P5** | `libunwind/RWMutex` 走 openkal;残余 `_WIN32` 清零;类型纪律进 conformance | ✅ SPEC §5.4 + §9.4 + `tools/check-types.sh`(带阴性对照) |
| **P6** | 四个目标的 `same-source` 判据 + 真机运行进 CI | ✅ 构建在 Linux,产物在真 Windows / 真 macOS 上跑,**那两个 job 什么都不装** |
| **P7** | 宿主维度补测(macOS/Windows 宿主 → 其它目标) | ✅ `host-dimension` job;⭐ 写它时预测并修掉了「链接行按宿主分三支」 |
| **P8** | mcpp 从 capability 推导图级 flag | ✅ `graph_runtime_compile_flags`(`-fdwarf-exceptions` / `-femulated-tls`) |

---

## 8. 风险

| # | 风险 | 评估 |
|---|---|---|
| R1 | libc++ 的 OS 分派点比已知的多 | 中。⚠️ **只能靠逐目标构建暴露** —— 这正是 §6.2 判据存在的理由 |
| R2 | 上游重构挪动被覆盖的文件 | 低~中。⭐ 调研表明方向一致(locale 后端正在做成可插拔);⚠️ 每次同步 vendored 树要重核对 |
| R3 | 标记补丁随上游漂移 | 中。⚠️ 头文件覆盖漂移面为零,补丁不是 ⇒ **能用覆盖就别用补丁** |
| R4 | 「一个事实两条通道」还有第三处 | 中。⭐ **本轮按这条规矩主动去找,找到了两处**:`std` 模块的编译命令(已改为共用一个函数,不是说两遍),和链接行按宿主分的三支。⚠️ 仍应在每次改动后照做 |
| R5 | 目标 ABI 的差异比已知的多 | 中。⚠️ **只影响 C 库那一层**(§1.3),不影响 openkal 接口。数据模型已知两处(LLP64 / Apple `int64_t`),浮点 ABI、对齐规则未系统核对 |
| R7 | 上层悄悄用了后端类型 | ✅ **已关闭**。SPEC §5.4 立规,§9.4 定核验,`tools/check-types.sh` 问编译器解析到了什么(不是 grep),CI 每行都跑并**自证会红** |
| R6 | 平台实现的 stub 与真实系统不一致 | ✅ **已关闭**。`run-on-windows` / `run-on-macos` 两个 job 下载 Linux 上构建的产物并在真机上跑,判据是 `unwound: true` |

---

## 9. 与既有文档的关系

- `2026-08-22-ecosystem-closure-design.md` §9.1 定了**验收目标**,本文是它第 1 与
  第 3 条的完整实现方案。
- `2026-08-23-universal-cross-build-design.md` 是本文的第一版,**已被本文取代**。
- 本文**不涉及**「一个二进制跑遍所有 OS」(统一格式 / 安装期物化)。那是另一件事:
  这里做的是「**同一份源码**建给不同实现」,不是「**同一个二进制**在不同 OS 上跑」。
