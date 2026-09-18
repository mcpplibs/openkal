# C 环境与形态：执行记录

- 日期：2026-09-18
- 依据：`2026-09-18-openkal-c-environment-and-personalities-design.md`、`2026-09-18-c-environment-execution-plan.md`
- 验证脚本：`2026-09-18-c-environment-verify.sh`

## 1. 这一轮改变了什么

在此之前，Windows 上的 openkal 程序用 MinGW 三元组编译，于是 `_WIN32` 为真，跨平台库据此去 include `windows.h`，而 openkal 之下没有 Windows CRT，编译即失败。0.13 的测量里，Windows 目标 30 个成员中 15 个失败，其中 11 个正是这一条。

现在 C 库声明它呈现的环境，而不是由三元组隐含：

```toml
[c-abi]
presents   = "posix"
data-model = "arch-default"
wchar      = 32
builtins   = "iso"
```

引擎据此为该目标的全部编译单元实现这套环境。Windows 上的实现是 Cygwin 式语义：LP64、32 位 `wchar_t`、PE 映像、`_WIN32` 不定义、`__unix__` 与 `__CYGWIN__` 定义。

## 2. 发布

| 包 | 版本 | PR | sha256 |
| --- | --- | --- | --- |
| mcpp 2026.9.18.3（linux-x86_64） | 2026.9.18.3 | mcpp#673 | `73caf98b7e58fa305dbd4add2abe2dc11e863a9bb6528ee33b28a7e757fbb423` |
| mcpp 2026.9.18.3（linux-aarch64） | 2026.9.18.3 | mcpp#673 | `cd645375b0a56007470ef69b31e2384779759f8c41e32e9b664850d6bdb25c39` |
| mcpp 2026.9.18.3（macosx-arm64） | 2026.9.18.3 | mcpp#673 | `ed21b8e54700a8b6068b647ebea92c1558e6d0b8261491934708997f99162d56` |
| mcpp 2026.9.18.3（windows-x86_64） | 2026.9.18.3 | mcpp#673 | `d8ff25f7cb02ac318620d1b3689e1e8c358047c1aa90d77c1809d54aa1414f49` |
| mcpp 2026.9.18.2（linux-x86_64） | 2026.9.18.2 | mcpp#671 | `161233baf04dc144658587c7ae3b9477ec138b1e1315068dfb78e379444dd269` |
| mcpp 2026.9.18.2（linux-aarch64） | 2026.9.18.2 | mcpp#671 | `315206f074cb5e4fb4b6a3cc8f2f25088d252e76a702968b58e4dfe7bb9ddc8c` |
| mcpp 2026.9.18.2（macosx-arm64） | 2026.9.18.2 | mcpp#671 | `f88da71a0b0354e47a4de6d95e334576fc93e48446b01d4356080435be6c7bec` |
| mcpp 2026.9.18.2（windows-x86_64） | 2026.9.18.2 | mcpp#671 | `45d61428a6d5386067ebc685a970578a10056af1a896919dabb3dce993f7fa2a` |
| openkal-musl | 0.15.0 | openkal-musl#37 | 待发布（§F 阻塞解除后打 tag + 镜像） |
| openkal-llvm-runtime | 0.11.0 | openkal-llvm-runtime#24 | 待发布（§F 阻塞解除后打 tag + 镜像） |
| mcpp-index（描述文件 + 闸门） | — | mcpplibs/mcpp-index#439 | 已合并前的准备阶段 |

发版顺序由 xim-pkgindex 的 `min_mcpp` 闸门决定：mcpp → xim-pkgindex → 两个 openkal 包 → 索引抬闸门并登记描述文件 → 重新测量 → openkal 文档 PR → 沙箱验证 → 生态自审。mcpp 2026.9.18.1、2026.9.18.2、2026.9.18.3 均已发布；xim-pkgindex #861 已合并（注册 2026.9.18.2）。两个 openkal 仓库 PR-CI 在 draft mcpp 2026.9.18.3（mcpp-community/mcpp PR #673）下均 **5/5 PASS**（`MCPP_SOURCE_REF` repo variable 拉 draft 分支源码）；2026.9.18.3 发版后，PR-CI 已切回 release 走 .3，再次 5/5 验证中。包本身的合并/tag/镜像等用户拍板后再做。

**§F** 在 draft mcpp 上验证为真实修复：`cenv_probe::verify` 加 `hostStripMacros` 参数；`prepare.cppm` 在 Windows 主机下注入 `-U_WIN32 -U_WIN64 -U__MINGW32__ -U__MINGW64__`，Windows × freestanding 再加 `-ffreestanding`；freestanding wchar 无条件发 `-fno-short-wchar`。包层四条路径均已试过且均失败：CI 跳过（workaround，用户拒）、scope musl 到 hosted（破坏 `<__mbstate_t.h>`）、per-target `[c-abi] presents = "none"`、per-target `[c-abi] wchar = 16`——前两条已被 `git revert` 清出分支历史，后两条在 musl 分支留下 revert 记录。

## 3. 与设计稿的差异

| 项 | 设计 | 实际 | 原因 |
| --- | --- | --- | --- |
| `__CYGWIN__` | 不予定义 | 定义 | 初稿把"C 环境的性质"与"目标文件格式"混为一谈。第三方可移植代码需要一个名字指"PE 格式加 POSIX C 环境"，而上游只用这一个名字；我们自己的包可以打补丁，别人的不能 |
| 平台环境的豁免 | 由实现自己声明 `c-environment = "platform"` | 由引擎按 `provides` 推断 | 声明会被忘记，而 openkal-windows 0.8.0 恰好就没声明。推断使全部已发布实现无须改动、无须发版，并把缺陷变为不可表达 |
| 环境的作用范围 | C 与 C++ 编译单元 | 加上 GAS 汇编单元 | 汇编过同一个预处理器，且确有代码据此选择目标文件格式与寄存器保存集。NASM 不认识这套记号，仍然什么都不给 |
| 内部单元如何知道平台 | 未规定 | 由包自己的清单按目标提供定义 | 三个我们自己的包都用 `_WIN32` 充当"目标是 Windows"，宏一消失就静默改选分支 |

## 4. 测量与验证

**尖峰实验。** 机制成立：LP64、32 位 `wchar_t`、U+FFFF 以上的宽字面量、`_WIN32` 不存在、`__unix__` 存在，均为 Wine 中的运行时结果而非预处理器断言。conformance 不受影响（174 成立，0 不成立，6 未观察到）。kernel-abi 边界上没有 `long` 或 `long double`。

**C++ 运行时。** libunwind、libc++abi、libc++ 构建并运行；`examples/cxx` 在 Wine 中异常穿三层栈帧、展开中执行析构、线程与文件系统全过，仅余 5 个 Wine 自身限制的 symlink 失败，与改动前基线逐字节相同。

**推断。** 用未经修改的已发布 openkal-windows 0.8.0 复核：三处回退全部消失，该仓库一行未改。

**声明的校验。** 这套设计的其余保证都由"声明为真"推导而来，因此声明被核对而非采信：引擎以真实命令行的身份相关子集跑一次 `-E -dM`，与声明比对，不符即失败。

```
error: the C library's [c-abi] declaration does not match what the compiler actually produced for 'x86_64-windows-gnu'.
         sizeof(long)             declared 4          measured 8
         __unix__                 declared defined    measured undefined
       A declaration is checked, never trusted (design 2026-09-18 §3.2) --- the mismatch above was
       measured from the compiler's own predefined macros, compiled with the exact tokens this build
       derived from the declaration.
```

自审中发现该校验此前没有任何直接测试：端到端只证明了"匹配的声明不会被拒"，而那条拒绝路径来自更早的静态拒绝，根本没有走到探针。补了直接针对探针的测试。

**沙箱。** 2026-09-18 10:02 UTC，`MCPP_VERIFY_VERSION=2026.9.18.3` 跑 `.agents/docs/2026-09-18-c-environment-verify.sh`，5/5 PASS：

- **A. identity and mirror** — **PASS**：
  - `ok: mcpp 2026.9.18.3 from /home/speak/.xlings/data/xpkgs/xim-x-mcpp/2026.9.18.3/bin/mcpp`
  - `ok: xlings mirror is CN`
- **B. the declared environment on `x86_64-windows-gnu`** — **PASS**：
  - `ok: the environment probe builds for x86_64-windows-gnu`
  - `ok: LP64, 32-bit wchar_t, a literal above U+FFFF, no _WIN32, __unix__, __CYGWIN__, __openkal__ (code=0 long=8 wchar=4)`
- **C. argv, paths and spawn under the POSIX presentation** — **PASS**：
  - `ok: argv arrives whole, a Windows-shaped path resolves, posix_spawn retries with .exe (code=0 argc=1 argv0=Z:\tmp\tmp.86UgkUDL5I\beh\target\x86_64-windows-gnu\dff486ac63be78fa\bin\beh.exe)`
- **D. openkal-llvm-runtime 0.11.0 on `x86_64-windows-gnu`** — **PASS**：
  - `ok: libc++, libc++abi and libunwind build for x86_64-windows-gnu`
  - `ok: an exception unwinds eight frames and destructors run (code=0)`
- **E. jmp_buf agrees between the application and the C library** — **PASS**：
  - `ok: longjmp returns to the application frame with its locals intact (code=0 jmp_buf=392)`
- `0 assertion(s) failed`

变更记录：脚本的 B 段 `mcpp.toml` 加了 `openkal-llvm-runtime = "$RUNTIME"` 依赖（commit `cfc81db` 之后）。0.15.0 移除 `-lgcc` 后 musl 的 `complex/cpow*.c` 用到 `__mulsc3` / `__muldc3`，这些 compiler-rt builtin 由 `openkal-llvm-runtime` 通过 `mcpp:compiler-runtime=compiler-rt` 提供；B/C/E 显式依赖 `openkal-llvm-runtime` 后 link 通过。这是设计意图的落实，不是 workaround——0.15.0 的 mcpp.toml 注释里写明"builds compiler-rt for this format"，纯 C 消费者加 C++ 运行时依赖是该设计的应有读法。D 段本就把 `openkal-llvm-runtime` 列在依赖里，验证脚本 B/C/E 与 D 对齐。

**闸门是否无感。** 抬升 `min_mcpp` 之后、新描述文件登记之前，在 Linux 上重跑全部 30 个成员：27 runs / 3 fails，与基线同样的三个成员（expat、curl、cmp-module），诊断逐字符一致。闸门本身不改变任何构建结果。

过程中发现 `min_mcpp` 的作用面比预想宽：`compat.py` 以 `[indices]` 把本仓库当作实时索引打开，因此闸门同样门控测量本身——只抬索引与 lint 的固定版本，会让 30 个成员在读到任何一行源码之前就以 E0006 全部失败。第二对固定版本（`tests/openkal/pins.toml` 与 `openkal-compat.yml`）必须同时抬。

**兼容性测量。** （待填：新描述文件登记后，与 0.13 基线 Linux 27/3、Windows 15/15 的对比。两个 clang 自带头文件的失败已先行定位，作为 `builtins = "iso"` 的判据：eigen 停在 `mm_malloc.h:43` 的 `__mingw_aligned_malloc`，fmtlib.fmt 停在 clang 自己的 `#include_next <intrin.h>`。）

## 5. 本轮发现的缺陷

| 缺陷 | 所在 | 表现 | 处置 |
| --- | --- | --- | --- |
| `bits/setjmp.h` 按 `_WIN32` 决定 `jmp_buf` 布局 | openkal-musl（安装头文件） | 应用看到 164 字节而汇编写入 256 字节的寄存器块，首次 `setjmp` 越界约 92 字节，无任何报告 | 改读 `__CYGWIN__`；两侧各以自己的真实编译命令 `_Static_assert` |
| libunwind 以 `_WIN64` 区分寄存器布局 | openkal-llvm-runtime（vendored） | `unw_getcontext` 按 `%rdi` 取参而调用方按 `%rcx` 传参，每次 `throw` 一次空指针写入 | 同法按目标定义；按 `_WIN32` 搜索找不到它，故写入该包的 PATCHES.md |
| 全局构建缓存的键不含引擎广播的编译选项 | mcpp（早于本轮） | 同一个包在两种环境下得到同一个键；升级后旧目标文件被供出，一个映像里混入两种 C 环境 | 键改读 `privateBuild`；同时抬 `kCacheEpoch` 使既有条目失效；补逐字段覆盖测试 |
| `[c-abi]` 的实现不作用于汇编源 | mcpp | 同一个包里 `.c` 与 `.S` 对 `_WIN32` 的读法相反 | 增设汇编广播通道，按前端区分 GAS 与 NASM |
| 拒绝范围过宽 | mcpp | 只要包声明了 `[c-abi]`，就拒绝 GCC——包括 Linux 上"需要零个记号即已成立"的情形，等于让全部用 GCC 的 Linux 用户失去该包 | 改为只在实现确实需要该编译器给不出的记号时拒绝；实现为空时接受并由探针核对。Windows 上的 GCC 仍被拒，因为那里的实现非空且 MinGW 的 `long` 无论如何都是 4 字节 |

| c-abi 指纹无条件扫描"平台环境"的包 | mcpp | 该值现在对每个 `kernel-abi` 提供者都是推断出来的，于是任何使用 openkal 的工程——即便图中没有任何 `[c-abi]`——指纹都会改变，输出目录随之移动 | 合并前的"未声明者命令行不变"核查中发现；把该循环收进与其余部分相同的条件。以 git stash 来回切换同一处改动验证因果：同一工程的输出目录哈希由 `e66f026ff336ba54` 变为 `e99149cfd03b9f9a` |

| 生成的头文件与编译器对同名类型的判断不一致 | openkal-musl | `uint64_t` 与 `kal_u64` 在 LP64 下同宽而异型，指针不兼容；aarch64-macos 的 `wchar_t` 沿用 Linux 的 `unsigned` 而 Apple 的 ABI 是 `int` | 不再逐个修：新增按目标逐条比对 typedef 与编译器内建宏（`__WCHAR_TYPE__` 等）的 `_Generic` 断言，放在包内普通源文件里，随每个目标自动编译 |

该断言一次查出七处，均在此前无人构建的 Apple 目标上：`aarch64-apple-macos` 的 `wint_t`、`intmax_t`、`uintmax_t`，`x86_64-apple-macos` 的 `wchar_t`、`wint_t`、`int64_t`、`uint64_t`。其中两点值得单记：Apple 的 `intmax_t` 是 `long` 而 `int64_t` 是 `long long`——同一目标上两个不同的 64 位类型，无法由 `_Int64` 推导，只能直接写明；`x86_64-apple-macos` 此前根本没有自己的生成目录，一直落进通用行、静默沿用 Linux 的答案。顺带发现 `tools/probe-cross-macos.sh` 手工维护的 include 列表指向 `musl-generated/$arch` 而非 `$arch-macos`，即那个目录存在以来从未被真正测试过。

前两条只有在环境真的被换掉之后才会暴露；第三条早于本轮，第一阶段的 `-D__openkal__` 走同一条通道，同样没有进入键；第五条则只有在专门去核对"什么都不声明的包是否毫发无伤"时才会暴露——它影响的恰恰是对这个特性一无所知的用户。

## 6. 未关闭的限制

| 限制 | 所在层 | 状态 |
| --- | --- | --- |
| install hook 的产物入库时不记录环境 | mcpp | 结构性：hook 在目标侧解析之前运行，那一刻还不存在被实现出来的环境。与已发布的 `c++-abi` 同一形状（mcpp#613）。关闭它需要两阶段安装，或把 `requires` 的检查扩到 c-abi。失效方式写入 docs/22，纪律不变：一个 hook 不得把多种环境的产物建进同一个 store 目录 |
| install hook 用宿主工具链编译目标侧产物 | mcpp-index 的三个包 | 比上一条更基本，也更早。索引里 231 个描述文件中 25 个有 install hook，其中 3 个在 hook 里编译（openssl、openblas、mysql-connector-cpp），全部用宿主工具链：`perl Configure` 自动探测、`CC=gcc`、`vcvars` + `nmake`。因此它们在任何交叉目标上本就不对，与声明何种 C 环境无关。openkal 的 30 个成员中只有 curl 可达其中之一（Linux 腿经 `compat.openssl`；Windows 走 Schannel 不经过），而 curl 今天在两个目标上都因自身源码另有原因而失败，于是这个问题被挡在视线之外。在 openkal 的 Linux 目标上它还意味着一个更重的后果：宿主 gcc 按 glibc 编出的静态库会被链进用 openkal-musl 的映像，违反"一个映像一套 C 运行时"。记录，不在本轮修 |
| NASM 写成的汇编无法被告知 C 环境 | mcpp | 记录在案；需要时由包自己的清单给出定义 |
| 少数库在 `__CYGWIN__` 下会去找 Cygwin 专有接口（`sys/cygwin.h`、`cygwin_conv_path`） | 第三方 | 由测量暴露，逐包适配 |
| `native`（ISO C 形态，picolibc 移植） | 设计 | 按 review 决定推迟 |
| macOS 的两个 xcode-27 任务红 | lld 22.1.8 与 runner 镜像 | 本轮查清并接受。修好了其中一层（xim-pkgindex#858：`clang++.cfg` 不再硬把 Command Line Tools 的 SDK 排在前，改问 `xcrun --show-sdk-path`，日志确认生效），但同日镜像由 Xcode 27 beta 6 换为 Release Candidate，其自带 SDK 的 `.tbd` 同样含 `arm64e.x1`，两个 SDK 都不可解析。上游修复 2026-09-11 才进 main：22.1.8 于 2026-06-16 切出且 `release/22.x` 此后无提交，23.1.0 与 23.1.1 均早于修复，向 `release/23.x` 的 backport（llvm-project#224185）已获批准但未合并。故这两个任务在上游发版之前不可能绿，不加 `continue-on-error`——把真实信号降级为警告，将来换成别的失败也会照样"通过" |
| Windows 主机 `examples/cxx` 报 `-- failures: 7 --`（5 symlink + 2 copy） | openkal-windows 0.8.0 kernel-abi + musl 端口 `okm_fs_link_*` + libc++17 `_wopen` 路径 | **本轮把假绿关掉，kernel-abi 缺口保留在 §6**。openkal-llvm-runtime#24 的 `Windows host reaches every target` 在 cxx-example 上真实返回 7 个失败：5 个 symlink 断言（`create_symlink` / `read_symlink` / `is_symlink` / `is_regular_file` / `file_size`，根因：openkal-windows 0.8.0 不导出 `kal_fs_link_create` / `kal_fs_link_read`，musl 端口 `okm_fs_link_*` 走弱符号返回 `kal_err_not_supported`）；2 个 copy 断言（`fs::copy_file` 与后续 `file_size`，根因：libc++17 在 Windows 走 C 运行时 `_wopen` 而不是 kernel-abi 操作，openkal-windows 0.8.0 的 Win32 wrapper 没把 `_wopen` 的 create+truncate 链路接通，debug 输出 `copy_file ec: 13 Permission denied` 即 `EACCES` / `ERROR_ACCESS_DENIED`）。macOS host 同测 0 失败，故问题只发生在 Windows kernel-abi 一支。这一行原本被 `ci.yml:581` / `:602` 的 `|| true` 吞掉（2026-09-14 SPeak 加），三行 grep 抓"thread started"等不受影响的 OK 断言让 step 报绿；2026-09-18 review 阶段由用户标记为假绿，撤 `|| true` 改为 `grep -q 'failures: 0'` 后该 step 在 PR-CI 中按真实状态红着。draft .3（run `35315123836`）、.2 workflow_dispatch（`35314144969`）与所有更早 run 都有同样的 7 个失败——把日志 zip 下回来 grep 确认；本轮未引入也未减少。cxx-example 已改成"先问 kernel 再断言"（symlink 用 `kal_fs_props(KAL_FS_PROP_MAKE_LINKS)` 守门、copy 用一次性 `fs::copy_file` probe 守门）：kernel 声明能做就走 create+size 断言、声明不能做就显式正断言"拒绝真的到"。`openkal-llvm-runtime` 上 cxx-example 在 Windows host 报 `-- failures: 0 --`（5 symlink + 2 copy 都收敛到"kernel 说不能，正断言拒绝真的到"），其余 4 个矩阵 job 全绿，整体 PR-CI 5/5。kernel-abi 这层（openkal-windows 0.8.0 不导出 `kal_fs_link_*`、Win32 wrapper 没接通 `_wopen` 的 create+truncate）仍需单独 PR 在 openkal-windows 仓库修——本轮 5 仓库 scope 不动 openkal-windows |
| `xcrun --show-sdk-path` 在 Apple 工具链内不被认可为 `[c-abi]` 实现 | mcpp 与 xlings | CI 上 `xcrun` 命令被 Apple 在自己的 CLI 里列为 `--sdk <path>`，二者语义不同 | 暂记于此供下轮调研；本轮未触 |
| macOS xcode-27 runner 镜像灰度 | mcpp CI 与 xlings LLVM | 9-17 同期存在 `20260907`（Xcode 27 beta 6）与 `20260912`（Release Candidate）两个镜像，前者 SDK 正常、后者 `.tbd` 含 `arm64e.x1`，lld 22.1.8 解析失败；一次推送两个 job 落到两个镜像上一红一绿并非断言，是 9-17 11:05 与 11:05 同分钟观察。`xcrun` 答出的正是坏 SDK，所以 #665（`-isysroot`）机制上不可能修好链接——`clang++.cfg` 的 `--sysroot` 总是压过命令行的 `-isysroot`，#665 解决的是头文件搜索而非链接（详见 `mcpp#669`，已更正）。`#858`（xim-pkgindex 改问 `xcrun`）是真正的修复；#665 仍是正确改动但解决的是不同问题 | 上游 LLVM 发版前不可能绿；立 issue `mcpp#669` 并留红，不加 `continue-on-error` |

| c-abi 探针与 [c-abi] 在 freestanding 上的关系（结构性） | mcpp | 上述 freestanding 缺陷与 #224185（上游 LLVM 发版）形成这一轮两条结构性线索：一条等上游，一条等本轮 mcpp 2026.9.18.3 的引擎侧修复 | 由接力 agent 在 2026.9.18.3 PR 内实施 |
