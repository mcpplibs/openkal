# openkal 生态级设计：三层三组宏、C 库形态，以及平台代码的显式化

- 日期：2026-09-18
- 状态：review 通过（2026-09-18，见 §12），精简 ISO C 形态推迟。不改动任何已发布的包。
- 前置：`2026-09-17-openkal-ecosystem-cross-repo-design.md`（分层、R1/R3）、`2026-09-17-openkal-0.13-ecosystem-record.md`（0.13 发布与实测数据）
- 涉及：`mcpplibs/openkal`（README 与文档）、`openkal-musl`、`openkal-llvm-runtime`、`mcpp-community/mcpp`、`mcpplibs/mcpp-index`

## 0. 结论摘要

1. **一个宏只回答一个问题。** 内核 ABI 由 `__openkal__` 陈述，C 环境由 `__unix__` / `_WIN32` 一类的宏陈述，系统由 `__linux__` 一类的宏陈述。今天 Windows 上的失败，根因是 `_WIN32` 同时回答了三个问题而其中一个是假的。
2. **环境由提供 C 库的那一层陈述，不由引擎内置，也不由程序重述。** openkal-musl 声明它在每个目标上都呈现 POSIX 环境；mcpp 负责实现与校验。
3. **Windows 上采用 Cygwin 式的编译环境语义**：PE 与 Win64 调用约定不变，不定义 `_WIN32`，数据模型 LP64，`wchar_t` 32 位。这让老库的 unix 分支自动生效，并删掉 openkal-musl 现有的四处补丁。
4. **C 库是可替换的形态，不是 openkal 的身份。** 现有 POSIX 形态（musl 移植）之外，规划精简 ISO C 形态（picolibc 移植），服务固件、wasm 与只用 `kal_*` 的新库。不自研 libc。
5. **平台代码显式化。** 使用平台接口是开发者的选择，通过 feature 引入平台依赖、隔离编译单元、边界只传定宽类型；不做 Win32 仿真层。
6. **闭包可见。** 构建报告列出平台依赖，清单可以拒绝平台依赖，产物可以断言导入表，索引用 `native` / `posix` / `platform` 三个实测标签表达同一件事。

## 1. 问题与证据

### 1.1 实测

2026-09-17 的兼容性测量（30 个成员，mcpp 2026.9.17.3、openkal-llvm-runtime 0.10.0）：

| 目标 | runs | fails | 失败构成 |
| --- | --- | --- | --- |
| x86_64-linux-gnu | 27 | 3 | curl 与 asio 需要 Linux 内核头文件或 epoll；expat 调用 musl 未声明的 `arc4random_buf` |
| x86_64-windows-gnu（Wine） | 15 | 15 | 11 个是包在 `_WIN32` 下 include 平台头文件；2 个是 clang 自带头文件按 MinGW 假定 C 库；2 个卡在依赖链 |

Windows 上 15 个失败里有 13 个的根因是同一个：环境宏说 Windows，实际环境是 POSIX。

### 1.2 `_WIN32` 同时代表三件事

库代码用它表示：可以调用 Win32 API；C 运行时是 Windows CRT；机器码是 PE 与 Win64。openkal-Windows 上只有第三件成立。

### 1.3 现状是靠补丁维持的

`openkal-musl/musl/PATCHES.md` 记录了四处改动，全部用于迎合 Windows 目标的 LLP64 与 16 位 `wchar_t`：五个生成的 `alltypes.h` 把 `_Addr` 改为 `long long`；三个源文件把 `L""` 改写成数组。同一份文件写明："上层程序写 `L"..."` 在 Windows 上会编译失败，这是被选择的结果。"

### 1.4 编译器已有的事实

本机 clang 22.1.8 核实：

| | `x86_64-w64-windows-gnu` | `x86_64-pc-cygwin` |
| --- | --- | --- |
| 格式 / 调用约定 / 异常 | PE / Win64 / SEH | PE / Win64（实测第五个参数取自 `40(%rsp)`）/ SEH |
| `long` | 4 字节 | 8 字节 |
| `wchar_t` | 16 位 | 默认 16 位，`-fno-short-wchar` 后 32 位 |
| 预定义 | `_WIN32`、`_WIN64`、`__MINGW32__` | `__CYGWIN__`、`__unix__`、`unix` |

"生成 Windows 机器码、对源码呈现 unix 环境"这一组合在编译器里已经存在，不需要自造。

## 2. 三层三组宏

| 宏族 | 陈述 | 由谁定义 | 例子 |
| --- | --- | --- | --- |
| 内核 ABI | `kal_*` 可用，且其行为与平台无关 | 提供 `mcpp:kernel-abi=openkal` 的层 | `__openkal__`、版本宏 |
| C 环境 | 源码面对的 C 环境形状 | 提供 `mcpp:c-abi=<impl>` 的层 | `__unix__`、`_WIN32`、`__MINGW32__` |
| 系统与架构 | 底层系统与处理器 | 目标三元组 | `__linux__`、`__APPLE__`、`__x86_64__` |

### 2.1 `__openkal__` 的规则

允许：用它决定是否调用 `kal_*` 的操作。它在所有目标上含义相同，不携带平台信息，因此不破坏"源码不识别具体实现"。

禁止：用它选择头文件、推断 `_WIN32` 的真假、绕开 SDK 缺失、区分 linux/windows/macos。这些属于 C 环境层或平台层，用 `cfg(c-abi = …)`、`cfg(kernel-abi = …)` 在清单里表达。

过渡手段：openkal 是纯声明包，`__has_include(<openkal/version.h>)` 今天即可用于同一目的；由层定义宏只是更直白。

**这修订 openkal 0.13 README 中"没有任何宏说明程序构建在 openkal 上"一句。** 修订后的表述：内核 ABI 层由 `__openkal__` 陈述，可用于选择是否调用 `kal_*`；环境差异由 C 环境层陈述，不得用 `__openkal__` 代替。

### 2.2 为什么不是引擎内置

引擎内置"c-abi 是 musl 且目标是 Windows 就换语义"会把某个实现的策略写进引擎，第二个 POSIX C 库出现时又要改引擎。程序重述则会与 C 库的实际编译方式矛盾。只有 C 库包知道它是按什么编译的，所以由它陈述，引擎执行并校验。

## 3. C 库形态

### 3.1 形态与入口

| 形态 | 包 | 提供 | 面向 |
| --- | --- | --- | --- |
| POSIX | `openkal-musl`（C）、`openkal-llvm-runtime`（C++） | `mcpp:c-abi=musl` | 老库、绝大多数现有代码 |
| 精简 ISO C（推迟，见 §12 决定 5） | `openkal-picolibc`、`openkal-llvm-runtime-picolibc` | `mcpp:c-abi=picolibc` | 固件、wasm、只用 `kal_*` 的新库 |
| 平台原生 | 不使用 openkal | 由平台提供 | 需要平台 C 运行时时，整个目标不走 openkal |

形态由程序选择，方式是依赖哪个入口包；库不得选择形态。一个镜像只有一个 C 库。

不自研 libc：老库需要的是 POSIX，自研要兼容老库就等于重写 musl；新库用 ISO C 加 `kal_*` 即可。精简形态用移植而非自研（picolibc 的系统入口接到 `kal_*`）。

### 3.2 C 库包声明它呈现的环境

```toml
# openkal-musl 的清单
[package]
provides = ["mcpp:c-abi=musl"]

# 这个 C 库在每个目标上都呈现同一种环境，因此不按目标分。
[c-abi]
presents   = "posix"        # 环境身份：定义 __unix__，不定义 _WIN32 / __MINGW32__
data-model = "arch-default" # 程序模型：该架构上 musl 自己的模型，64 位即 LP64
wchar      = 32             # 与 musl 的 wchar_t 一致
builtins   = "iso"          # 编译器不得假定平台 C 库的扩展，见 3.2.1
```

三个键分属两类事实，互不推导：`presents` 决定源码走哪条分支，`data-model` 与 `wchar` 决定 ABI。POSIX 不蕴含 LP64（32 位架构上是 ILP32），LP64 也不蕴含 POSIX。

`presents` 的取值是封闭集合，拼错是错误而不是被静默忽略：

| 值 | 含义 |
| --- | --- |
| `posix` | 定义 `__unix__`，不定义 `_WIN32` 与 `__MINGW32__` |
| `windows` | 定义 `_WIN32` 一族，即平台自身的 C 运行时 |
| `none` | 不定义任何环境身份宏；只有 ISO C 的形态用它 |

**只有提供该层的包可以声明。** 一个不提供 `mcpp:c-abi` 的包写这个块是错误，因为它在陈述一件自己不知道的事。

**声明被校验，而不是被信任。** 引擎在解析出目标侧之后，用最终的编译参数编译一个探针翻译单元，核对 `sizeof(long)`、`__SIZEOF_WCHAR_T__` 以及 `_WIN32` 是否存在，与声明不符即失败并同时报出声明与实测。这与 openkal 自己的做法一致：声明不是机制，检查才是。

不声明时保持今天的行为，即三元组说什么就是什么，因此对现有包向后兼容。

#### 3.2.1 编译器对平台 C 库的假定

环境身份宏只解决预处理阶段。编译器还会因为目标三元组而假定平台 C 库提供某些设施，这些假定发生在预处理之后，宏改不到：

| 平台 | 假定 | 发生阶段 | 现象 |
| --- | --- | --- | --- |
| macOS | libSystem 提供 `memset_pattern16` | 代码生成（循环惯用法识别） | 链接期缺符号，`-U__APPLE__` 够不到 |
| Windows | MinGW 提供 `__mingw_aligned_malloc`、`intrin.h` | 预处理（编译器自带头文件） | `mm_malloc.h` 与 `intrin.h` 编译失败 |

这两者是同一类问题：**编译器认为平台 C 库在场，而在场的是图里的 C 库**。因此 `[c-abi]` 增加一个键：

| `builtins` | 含义 | 实现 |
| --- | --- | --- |
| `iso` | 编译器只能假定 ISO C 与编译器自带的设施 | 关闭平台专有的惯用法识别（如 `-fno-builtin-memset_pattern16`），并在需要时由 C 库补上编译器坚持要的符号 |
| `platform`（默认） | 保持今天的行为 | 无动作 |

判据与 §3.2 的一致：声明被校验而不是被信任——尖峰实验中用一个会触发该惯用法的翻译单元验证链接结果。

### 3.3 mcpp 的映射

引擎保存"请求的性质 → 目标三元组与开关"的映射，这是通用知识，不含包名。

| 目标 | 请求 | 实现 |
| --- | --- | --- |
| Linux | posix / arch-default / 32 | 默认三元组已满足 |
| macOS | posix / arch-default / 32 | 默认三元组已满足 |
| Windows | posix / arch-default / 32 | 采用 Cygwin 式语义（LP64、PE、Win64），去掉 `__CYGWIN__`，加 `-fno-short-wchar` |
| macOS | builtins = iso | 关闭 `memset_pattern16` 一类的平台惯用法 |
| 无法满足 | | 明确拒绝并说明缺什么，不静默降级 |

`__CYGWIN__` 不予定义：它会把少数库引向 Cygwin 专有接口（`sys/cygwin.h`、`cygwin_conv_path`），而这些接口不在依赖图里。是否定义 `__unix__` 由尖峰实验的数据决定（§8）。

### 3.4 环境的作用范围，以及自身在平台环境里编译的包

声明的环境作用于**目标侧的全部编译单元**：C 库自己、C++ 运行时、compiler-rt 的 builtins，以及图中所有普通包。三者必须一致，否则 `long` 宽度会在它们之间错位。

有两类包必须例外，它们本来就编译在平台环境里：

| 包 | 为什么 |
| --- | --- |
| 提供 `mcpp:kernel-abi=openkal` 的实现（openkal-windows 等） | 它要 include 平台声明，`_WIN32` 对它必须为真 |
| 声明了平台依赖的包中的平台编译单元（§5.3） | 同上 |

因此包（或包内的某些文件）需要能够声明"我在平台环境里编译"：

```toml
[package]
provides = ["mcpp:kernel-abi=openkal"]
c-environment = "platform"     # 本包自身的单元按平台环境编译
```

`__openkal__` 由引擎在 `kernel-abi` 解析为 `openkal` 时定义，作用于目标侧全部单元。它取自层的取值而不是包名，因此引擎不会因为多了一个 openkal 实现而需要修改。

这是 §5.4 所说能力的另一半：环境可声明，粒度从图细化到包与文件。跨越这条边界的接口只能使用定宽类型，`kal_*` 已经满足（SPEC §5.4），平台 shim 的接口由本设计要求满足。

**已安装到 store 的构建产物必须把环境计入键。** mcpp 文档记录过一个同形问题：install hook 编译出的静态库按包与版本入库，不含 C++ 运行时的选择，因此这类包必须用 `requires` 声明它是为哪个实现构建的。环境同理：同一个包在 LP64 与 LLP64 下的产物不可互换，环境要么进入 store 键，要么由该包用 `requires` 声明。否则第一个消费者会替后来的所有消费者决定模型。

## 4. Windows 上的具体形状

| | 现在 | 本方案 |
| --- | --- | --- |
| 机器码 | PE / Win64 / SEH | 不变 |
| 环境宏 | `_WIN32`、`_WIN64`、`__MINGW32__` | 不定义；定义 `__unix__`（待实验确认） |
| 数据模型 | LLP64 | LP64 |
| `wchar_t` | 16 位 | 32 位 |
| openkal-windows | windows-gnu 编译 | 不变；边界只有定宽的 `kal_*`（SPEC §5.4） |
| musl 补丁 | 四处 | 删除 |

两个模型在同一镜像中共存是安全的，因为跨越边界的只有 `kal_*`，而 SPEC §5.4 规定它只用定宽类型。这一条当初就是为"数据模型不同的两侧在同一边界相遇"写的。

## 5. 平台相关代码

### 5.1 原理

系统 DLL 与 dylib 导出的是普通 C 函数，可以直接调用。障碍只有两个：平台 SDK 的头文件与 C 库头文件缠绕，不能出现在同一个编译单元；边界上的类型必须定宽。

### 5.2 允许与禁止

| 允许 | 禁止 |
| --- | --- |
| 调用系统 DLL/dylib 导出的 C 函数 | 使用平台 C 运行时的函数与对象 |
| 传句柄、定宽整数、定宽结构 | 传 `FILE*`、`long`、`wchar_t`，或跨边界的所有权 |
| 在调用者自己的线程里接收回调 | 在平台库创建的线程里回调进 C 库代码 |
| | 链接按平台 C 运行时编译的静态库 |

一个镜像仍然只有一个 C 库。系统 DLL 内部链接的 CRT 被关在 DLL 边界里，不构成第二个 C 运行时进入镜像。

macOS 是需要注意的例外：框架自身链接 libSystem，即该平台的 C 库，因此使用框架天然会把第二个 C 运行时带进进程，只能在上表的约束下使用。

### 5.3 两种实现方式

**包内 feature**，适用于只有一个包需要时：

```toml
[features]
default = ["posix-socket"]
posix-socket = { defines = ["TINYHTTPS_POSIX_SOCKETS"] }
winsock      = { defines = ["TINYHTTPS_WINSOCK"] }

[feature-deps.winsock]
win32-headers = "1.0"      # 平台 SDK 由依赖图提供，且不传给下游
```

**独立 shim 包**，适用于多个包需要同一段平台代码时：内部是平台编译单元，对外只暴露定宽类型的 C 接口。openkal-windows 本身就是这个形状。已知的第一个候选是 Windows 上的 DNS 服务器列表（`GetNetworkParams`），用于补上 musl 解析器缺少的 `resolv.conf`。

### 5.4 mcpp 需要补的能力

今天可以按文件 glob 给不同的编译参数，但不能给某几个文件换一整套环境（不同的数据模型与环境宏）。平台编译单元与 POSIX 代码共存于一个包需要这个能力。它是 §3.2 的自然延伸：环境是可声明的，粒度从包细化到文件。

## 6. 闭包的可见性

| 手段 | 状态 | 说明 |
| --- | --- | --- |
| 构建报告列出各层来源 | 已有 | `Target` 行打印 kernel-abi、c-abi、c++-abi 各自来自哪个包 |
| 报告列出平台依赖 | 建议新增 | 图中自带平台 SDK 的包；没有则显示为空 |
| `requires = ["mcpp:kernel-abi=openkal"]` | 已有 | 解析不到就在编译前拒绝 |
| `[build] platform-dependencies = "refuse"` | 建议新增 | 任何包引入平台 SDK 即失败，这是"我要完全基于 openkal 的闭包"的机器表达 |
| 产物断言 | 建议新增（CI 层面） | 断言最终镜像的导入表只含 openkal 实现所需的项。openkal 各实现已有同类检查（目标文件不引用任何 C 库符号） |

## 7. 索引标签

三个实测标签，回答"这个包需要什么环境"：

| 标签 | 含义 | 判据 |
| --- | --- | --- |
| `native` | 只用 ISO C/C++ 加 `kal_*` | 在精简 ISO C 形态下构建通过，依赖图中没有 POSIX 形态的包 |
| `posix` | 需要 POSIX C 库 | 在 POSIX 形态下测得 `runs` |
| `platform` | 自带平台依赖 | 解析结果中存在经 feature 引入的平台依赖，并测得 `runs` 或 `builds` |

没有标签表示未通过或未测量，详情页展示每个目标的状态与首条诊断。openkal 自身的包不参与这三个标签，单独分组，因为它们回答的是身份而不是需求。

标签之间是包含关系：`native` 的包在 POSIX 形态下同样可用，取其能达到的最强标签。

**`native` 的判据依赖精简 ISO C 形态，而该形态已推迟（§12 决定 5），因此这个标签暂不启用。** 在它可测之前，索引只使用 `posix` 与 `platform` 两个标签，不用源码扫描之类的近似判据去冒充它：一个没有测量支撑的标签，和没有标签相比只是更容易被相信。

## 8. 尖峰实验

在本地与 Wine 中进行，不改动已发布的仓库。

| 步骤 | 内容 | 判据 |
| --- | --- | --- |
| 1 | 用 `x86_64-pc-cygwin` 加 `-fno-short-wchar` 构建 openkal-musl（LP64，移除 LLP64 与 `L""` 补丁），链接 openkal-windows（仍为 windows-gnu） | 构建通过；`examples/subprocess` 等探针在 Wine 中全绿 |
| 2 | 同一环境运行 openkal conformance | 观察数与 windows-gnu 下一致 |
| 3 | 用该环境重跑 30 个成员 | Windows 上 15 个失败中自动转绿的数量；有无新增失败 |
| 4 | 对比两种呈现：定义 `__unix__` 与仅不定义 `_WIN32` | 哪一种转绿更多，是否有库因 `__unix__` 走进不适用的分支 |
| 5 | 核对未知数 | TLS 模型、C++ 异常展开、libc++ 能否在该环境构建、导入库与链接驱动 |
| 6 | 跨模型边界 | compiler-rt 的 builtins 与 openkal-windows 分处两种数据模型，核对两者之间没有以 `long` 或 `long double` 传参的接口 |
| 7 | 宽字符与宽流 | `std::wstring`、`std::wcout`、`wcslen` 在 32 位 `wchar_t` 下成立；与平台宽接口之间的转换只出现在平台单元中 |
| 8 | 名字与路径 | 用户给出的 `C:\\foo` 一类名字在 POSIX 呈现下如何解析；程序名的 `.exe` 后缀（musl 的启动已有重试）|

任一步不通过即记录原因并停止；本方案退回 §9 的第 1 条路径。

## 9. 备选与不做

| 备选 | 结论 |
| --- | --- |
| 维持现状，逐包按 `c-abi` 适配 | 保留为退路。成本与包数成正比（索引约 150 个 compat 包，抽样一半有 `_WIN32` 分支），且有静默错误风险（xz 去掉 `_WIN32` 后在 LLP64 上解码出错） |
| 三元组不变，由 mcpp 统一改预定义宏 | 不采用。数据模型无法用开关改变，且与"环境由 C 库层陈述"冲突 |
| 在 musl 上仿真 `windows.h` | 不做。永远做不完，且是 SPEC §3.1 拒绝的"模拟" |
| 自研 libc | 不做。见 §3.1 |
| 因 Windows 失败数推动 `openkal.event` | 不做。事件循环的缺失是按设计拒绝的，与本方案无关 |

## 10. 迁移与版本影响

| 包 | 变化 | 性质 |
| --- | --- | --- |
| openkal | README 第 4 条修订；`__openkal__` 的规则写入文档 | 文档，不改声明 |
| openkal-musl | Windows 上的数据模型与 `wchar_t` 改变；删除四处补丁；新增 `[c-abi]` 声明 | 破坏性，需新版本；该目标上的所有包重建一次 |
| openkal-llvm-runtime | 重新编译 | 跟随 |
| openkal-windows | 不变 | 无 |
| mcpp | 性质到三元组的映射；平台依赖报告；`platform-dependencies` 开关；按文件的环境 | 新增能力，向后兼容 |
| mcpp-index | 三个标签；测量在两种形态下各跑一次；撤回 zlib 与 mbedtls 中因 `_WIN32` 而加的适配 | 跟随 |

openkal 规范本身不变：本方案不新增也不修改任何 `kal_*` 声明。

## 11. review 决定（2026-09-18）

| # | 决定 | 结果 |
| --- | --- | --- |
| 1 | `__openkal__` 由内核 ABI 层定义，修订 README 第 4 条（§2.1） | 采纳 |
| 2 | 环境由 C 库包声明，用一个 `[c-abi]` 块（§3.2） | 采纳 |
| 3 | Windows 上采用 Cygwin 式语义、LP64、32 位 `wchar_t`（§4） | 采纳，以尖峰实验数据为准 |
| 4 | 是否定义 `__unix__` 由实验数据决定（§8 第 4 步） | 采纳 |
| 5 | 精简 ISO C 形态用 picolibc 移植 | **推迟**：方向保留，本轮不做，等出现真实消费者再评估。因此 `native` 标签暂不启用（§7） |
| 6 | 索引标签 `native` / `posix` / `platform`（§7） | 采纳，`native` 随决定 5 推迟 |
| 7 | 平台依赖报告与 `platform-dependencies = "refuse"`（§6） | 采纳 |

## 12. 自我审查记录

审查在 review 之后进行，发现五处缺口，均已写入相应章节。

| # | 缺口 | 影响 | 处理 |
| --- | --- | --- | --- |
| 1 | 原稿没有说环境的作用范围，也没有说 openkal-windows 自己怎么办。它必须在 `_WIN32` 为真的环境里编译，否则按图一律呈现 POSIX 会把它自己弄坏 | 会使方案无法实施 | 新增 §3.4：环境作用于目标侧全部单元；提供 kernel-abi 的包与平台单元声明 `c-environment = "platform"` |
| 2 | 声明与事实可能不符（包按 LLP64 编译却声明 LP64） | 静默的 ABI 错位 | §3.2：引擎用探针翻译单元核对 `sizeof(long)`、`wchar_t`、`_WIN32`，不符即失败 |
| 3 | `presents` 的取值没有定义，也没有说谁有资格声明 | 拼错被静默忽略；无关的包也能声明 | §3.2：封闭取值集合 `posix` / `windows` / `none`；只有提供该层的包可以声明 |
| 4 | install hook 产物按包与版本入库，不含环境 | 第一个消费者替其余消费者决定数据模型 | §3.4：环境进入 store 键，或由该包用 `requires` 声明 |
| 5 | `native` 标签的判据依赖尚未存在的精简形态 | 会出现无法测量的标签 | §7：该标签暂不启用，不用近似判据冒充 |

另外补了三条尖峰实验项（§8 第 6 至 8 步）：跨数据模型边界的 builtins、32 位 `wchar_t` 下的宽流、Windows 风格路径与 `.exe` 后缀在 POSIX 呈现下的行为。

仍然存在、但属于已知且被接受的风险：

| 风险 | 说明 |
| --- | --- |
| Cygwin 目标的成熟度 | TLS、异常展开、链接驱动，由尖峰实验第 5 步判定；不通过则退回逐包适配 |
| `__unix__` 可能带来新的失败 | 有些库的分支是 `__linux__` / `__APPLE__` / `#error` 三选一，通用 unix 会落到最后一支；由第 3、4 步的数据判定 |
| 一次破坏性变更 | openkal-musl 需要新版本，该目标上的包重建一次；openkal 规范本身不变 |
