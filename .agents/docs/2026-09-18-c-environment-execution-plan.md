# C 环境与形态：执行计划

- 日期：2026-09-18
- 依据：`2026-09-18-openkal-c-environment-and-personalities-design.md`（review 通过，决定 1 到 7，决定 5 推迟）
- 原则：每个仓库一个 PR；尖峰实验先行，数据不通过就停在实验 PR，不进入落地。

## 1. 清单形状（两个 agent 必须一致）

C 库层的包（openkal-musl）声明：

```toml
[package]
provides = ["mcpp:c-abi=musl"]

[c-abi]
presents   = "posix"          # posix | windows | none
data-model = "arch-default"   # arch-default | lp64 | llp64 | ilp32
wchar      = 32               # 16 | 32
builtins   = "iso"            # iso | platform
```

自身在平台环境里编译的包声明：

```toml
[package]
c-environment = "platform"
```

提供 `kernel-abi` 的实现（openkal-windows 等）不在此列：引擎按 `provides` 推断，不需声明，显式声明仍然优先（P4 复评，设计稿 §3.4）。

引擎在 `kernel-abi` 解析为 `openkal` 时，为目标侧全部单元定义 `__openkal__`。

## 2. 任务与依赖

```
 P0  设计稿补充（已完成）与本计划
 P1  mcpp：环境声明、映射、校验、指纹与 store 键、__openkal__、平台依赖报告与拒绝开关
 P2  openkal-musl：LP64 与 32 位 wchar 的 Windows 头文件，移除四处补丁，声明 [c-abi]
 P3  尖峰实验：P1 的二进制 + P2 的分支，跑探针、conformance、30 个成员、两种呈现对比
 P4  判定：数据通过则继续；不通过则停在实验 PR，退回逐包适配
 P5  落地：mcpp 发布；openkal-musl 0.15.0；openkal-llvm-runtime 0.11.0；openkal 文档修订
 P6  mcpp-index：min_mcpp 与 CI 固定版本同时抬升、pins、标签、撤回不再需要的 _WIN32 适配、重新测量
 P7  发布（GitHub 标签 + GitCode 镜像）、沙箱验证、生态级自审
```

P1 与 P2 可以并行；P2 的验证需要 P1 的二进制。

## 3. 判据

| 阶段 | 判据 |
| --- | --- |
| P1 | 单元测试覆盖声明的解析、校验、拒绝；未声明的包命令行逐字节不变；e2e 用一个声明了环境的假包验证映射生效 |
| P2 | 在 P1 的二进制下，musl 为 `x86_64-windows-gnu` 构建通过，探针在 Wine 中全绿 |
| P3 | 30 个成员的测量：Windows 上 15 个失败中转绿的数量；有无新增失败；两种呈现（定义 `__unix__` 与否）的对比 |
| P5 | 各仓库 CI 全绿 |
| P6 | `compat.zlib` 的 `-U_WIN32 -include unistd.h` 与 `compat.mbedtls` 的 `-U_WIN32 -D__unix__` 撤回后成员仍然通过——这两处是 `presents = "posix"` 的手工前身，撤得掉才说明声明真的生效；Windows 行的 runs 数不低于测量基线 |
| P7 | 沙箱中只写版本号即可解析并构建；导入表断言 |

## 4. P4 判定（第一次，2026-09-18）

尖峰实验的数据：机制本身成立（LP64、32 位 wchar、U+FFFF 以上的宽字面量、`_WIN32` 不存在、`__unix__` 存在，均在 Wine 中实测）；conformance 不受影响（174 成立，0 不成立，6 未观察到）；kernel-abi 边界上没有 `long` 或 `long double`。两处不通过：

| 现象 | 根因 | 处置 |
| --- | --- | --- |
| libunwind `config.h:117 #error Unsupported target`，C++ 运行时无法构建 | openkal-llvm-runtime 已有的补丁以 `_WIN32` 选中 libunwind 的 PE 分支（`RWMutex.hpp:29`、`UnwindCursor.hpp:33`、`AddressSpace.hpp:112,592,686`），宏消失即连自己的分支一并关掉 | 同下：由包的清单按目标提供定义；另补 `_LIBUNWIND_WEAK_ALIAS` 的 PE 分支 |
| argv 截断、Windows 式路径 ENOENT、`posix_spawn` EINVAL | openkal-musl 的 port 层以 `_WIN32` 识别平台（`okm_start.c`、`okm_format.c`、`okm_spawn.c`，以及安装头文件 `bits/setjmp.h`） | 由包的清单按目标提供定义（设计稿 §3.4 已补） |

两处其实是同一处：**我们自己的三个包都用 `_WIN32` 充当"目标是 Windows"的代名词**，而新环境只承诺它不再代表"C 环境是 Windows 式的"。受影响的是 openkal-musl 的 port 层与 openkal-llvm-runtime 的 libunwind 补丁，都是我们有权修改的代码，修法相同：包在自己的清单里按目标给出定义。第三方代码不适用此法，它们的残差由 30 个成员的测量暴露并逐包适配。

判定为继续，P4 在重新测量后复评，停止条件收窄为一条：若 libunwind 的 PE 路径存在 POSIX 呈现下无法抵达的 Win32 调用，则停在实验 PR，退回逐包适配。

**复评（同日）：通过。** libunwind、libc++abi、libc++ 构建并运行；`examples/cxx` 在 Wine 中异常穿三层栈帧、展开中执行析构、线程与文件系统全过，仅余 5 个 Wine 自身限制的 symlink 失败，与改动前基线逐字节相同。`jmp_buf` 的隐患得到确证并修复：回退状态下应用看到 164 字节而汇编写入 256 字节的寄存器块，首次 `setjmp` 即越界约 92 字节；修复后两侧均为 392 字节，`_Static_assert` 分别以各自的真实编译命令核对。

复评还改正了三处回退的归属：它们不在 openkal-musl，而在 openkal-windows 被一并套进 POSIX 呈现（设计稿 §3.4）。处置不是给该仓库补一行声明，而是由引擎对 `kernel-abi` 提供者推断，理由见设计稿。

另有一处只有在汇编也进入替换之后才暴露的缺陷：libunwind 的 `UnwindRegistersSave.S`、`UnwindRegistersRestore.S` 与 `__libunwind_config.h` 以 `_WIN64`（而非 `_WIN32`）区分 Win64 与 SysV 的寄存器布局，`unw_getcontext` 于是按 `%rdi` 取参而真实调用方仍按 `%rcx` 传参，每次 `throw` 都是一次空指针写入。按 `_WIN32` 搜索找不到它。

## 5. 落地顺序

顺序由索引的 `min_mcpp` 闸门决定，不能并行：

| 步 | 动作 | 为什么在这一步 |
| --- | --- | --- |
| 1 | mcpp #668 合并并发版 | `[c-abi]` 的实现必须先存在于已发布的引擎里 |
| 2 | mcpp 进入 xim-pkgindex | 索引抬升闸门前，新引擎要装得到 |
| 3 | openkal-musl 0.15.0、openkal-llvm-runtime 0.11.0 发布（GitHub 标签 + GitCode 镜像） | 两者互不依赖，可同时 |
| 4 | mcpp-index 单 PR：抬 `min_mcpp` 与 `MCPP_VERSION`、登记两个新版本、撤回 zlib 与 mbedtls 的 `-U_WIN32` 适配、重新测量 | 闸门与描述文件必须同时生效，否则旧引擎会静默误读 |
| 5 | openkal 仓库单 PR：README 宏规则与本轮文档 | 只改文档，SPEC 未变，故无需发版 |
| 6 | 沙箱验证（`2026-09-18-c-environment-verify.sh`）、生态级自审 | 只能在索引发布完成后进行 |

openkal-windows 等实现不在此列：推断使它们无须改动。这一条已用**未经修改的已发布 0.8.0** 验证：`env.cpp`、`fs.cpp`、`process.cpp` 的编译命令只剩一个 `--target=x86_64-w64-windows-gnu`（读自 `compile_commands.json`），三处回退全部消失——argv 完整（192 字符，含空格的一项 14 字符不差）、`stat("C:\\Windows\\win.ini")` 返回 0、`posix_spawn` 重试 `.exe` 后子进程以 7 退出。第 6 步在沙箱中复核同一组断言。

## 6. 顺带修好的既有缺陷

全局构建缓存的键（`mcpp.build.cache_key::fill_package_config`）读包**清单里声明的** `cflags`/`cxxflags`，而引擎广播给包的那一份走的是 `privateBuild.cflags`/`cxxflags`。于是同一个包在两种被实现出来的环境下得到同一个键。相邻的 include 目录轴早已改读 `privateBuild`，正说明这是遗漏而非取舍。

这个缺陷早于本轮：第一阶段的 `-D__openkal__` 走同一条广播通道，同样没有进入键。它的后果不是构建变慢，而是一个映像里混入两种 C 环境且无任何诊断——正是本设计要守住的那条不变量。

把键修对并不能追溯区分"修复前写下的条目"与"修复后写下的条目"，而 `kernel-abi` 推断恰恰让最要紧的那个包新旧键相同（它的 `privateBuild.cflags` 两边都是空的）。因此同时抬升 `kCacheEpoch`（2 → 3）——它本就是为缓存格式不兼容而设、与 mcpp 版本号解耦，且有同类先例（mcpp#344）。代价是升级后第一次构建是冷的，这一点以用户能读懂的话写进 CHANGELOG。

另补一个覆盖测试，逐个断言 `privateBuild` 的每个广播字段都进入键，并把尚无广播的两个字段反向断言，要求将来加广播时在同一次改动里翻转断言。C++ 没有反射能让它自动失效，这是最接近的替代。

## 7. 升级的可读性

`[c-abi]` 是新增的清单表，而旧引擎"忽略清单中不认识的键"（mcpp `docs/22-target-side.md`）。因此旧 mcpp 读 openkal-musl 0.15.0 会静默地按旧办法构建，用户看到的是 LP64 头文件与 MinGW 编译器对不上的内部错误，而不是一句"引擎太旧"。这正是索引的 `min_mcpp` 闸门要防的那类静默失配（E0006）。

P6 因此必须与发布同时抬升 `index.toml` 的 `min_mcpp` 与 CI 的 `MCPP_VERSION` 固定版本——按 index.toml 自己写下的规则，两者只能一起动。代价是索引的全部使用者都要升级一次 mcpp，换来的是失败可读：E0006 直接说明要升级，并给出命令。

## 8. 风险与退出

- Cygwin 目标的 TLS、异常展开、链接驱动不可用：已测，异常展开与 `longjmp` 均成立，此项关闭。
- `__unix__` 带来新的失败超过收益：改为只不定义 `_WIN32`。由 P6 的 30 个成员测量裁决，`__CYGWIN__` 同理。
- 破坏性变更：openkal-musl 新版本，该目标上的包重建一次；openkal 规范不变。
- 索引闸门抬升后，未升级 mcpp 的使用者被 E0006 挡住：这是设计中的可读失败，不是回归。
- macOS 的 xcode-27 两个任务在 main 上即为红（runner 镜像的 Command Line Tools SDK 的 `.tbd` 含 `arm64e.x1`，lld 22.1.8 无法解析）。与本轮无关，但"CI 全绿"要求它有个归宿：mcpp 自己的编译已由 mcpp#665 改用 `xcrun --show-sdk-path` 绕开，剩下的是 xlings 的 LLVM 包默认 sysroot 仍指向 CLT。同一手法应可用于该包，列为本轮的生态尾项。
