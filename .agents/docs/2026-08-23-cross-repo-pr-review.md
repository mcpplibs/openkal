# 八个仓库、八个 Pull Request 的跨仓库 review

**日期**:2026-08-23
**范围**:openkal · openkal-llvm-runtime · openkal-musl · openkal-windows ·
openkal-macos · openkal-opensbi · openarch · mcpp(#486)
**方法**:两轮。第一轮按维度纵览全部改动并记录发现;修复;第二轮复核,并检查
第一轮的修复本身是否引入了新的问题。本文档在两轮进行中持续更新。

---

## 0. 为什么是「跨仓库」而不是八次单仓 review

这八个 PR 是**一件事**分在八个仓库里:一份规范、一个 C 库、一个 C++ 运行时、
四个平台实现、一个构建工具。单仓 review 能看见每个改动是否自洽,看不见三类问题,
而这三类恰好是本轮实际发现的:

1. **同一个事实被陈述两遍**,两处分别正确而彼此不一致(例:`-lgcc` 在
   `openkal-musl` 的清单里,而 compiler-rt 的构建在 `openkal-llvm-runtime` 的
   清单里 —— 只有同时看两份才知道 Linux 那一档在借另一个编译器的运行时)。
2. **判据写在错误的层**(例:平台包用「宿主是不是 macOS」决定要不要供给
   `libSystem` stub,而真正决定它的事实在 mcpp 那一侧)。
3. **一个仓的修复暴露另一个仓的缺陷**(例:mcpp 把产物格式判据改成按目标之后,
   `openkal-llvm-runtime` 的 macOS 链接才露出它一直落在 `Elf` 那一格)。

⇒ review 的单位是**这条链**,不是每个仓库。

---

## 1. 维度与判据

| # | 维度 | 这一轮用的判据 |
|---|---|---|
| A | 架构 | 每个决定是否落在**知道那个事实的那一层**;有没有跨层的硬编码 |
| B | 稳定性 | 失败是否可复现、可诊断;有没有「静默的错答案」 |
| C | CI 覆盖 | 断言是否**可能失败**;有没有恒绿的步骤 |
| D | 设计合理性 | 新增的机制是否**减少**了特例而不是增加 |
| E | 跨平台 | 有没有按「哪台机器在构建」而不是「输出给哪台机器」分的支 |
| F | 简洁优雅 | 同一个事实是否只有一个来源 |
| G | 兼容性 | 对旧版 mcpp、旧版索引、既有使用者是否仍然成立 |
| H | 扩展性 | 加第五个平台/第四个格式要改几处 |
| I | **测试集覆盖度** | `mcpp test` 单元测试与 e2e:改动的行为有没有一个**便宜且确定**的测试 |
| J | 文档与注释 | 陈述句、学术风;不使用口语与网络用语 |

---

## 2. 第一轮发现

### 2.1 ⭐⭐ I(测试覆盖):mcpp 改了 16 个源文件,新增测试为零

| | 数量 |
|---|---|
| 本 PR 改动的 `src/` 模块 | 16 |
| 新增单元测试 | **0** |
| 新增 e2e | **0** |
| 仓库既有 e2e 脚本 | 275 |
| 仓库既有单元测试文件 | 92 |

而**三个新增或改动的纯函数各自都有一个现成的测试文件就在旁边**,覆盖为零:

| 函数 | 现成的测试文件 | 该文件行数 | 对它的覆盖 |
|---|---|---|---|
| `Triple::llvm_triple()` | `tests/unit/test_toolchain_triple.cpp` | 258 | **0** |
| `graph_runtime_compile_flags()` | `tests/unit/test_hostflags.cpp` | 218 | **0** |
| `MechanismInput::graphCxxRuntime` 短路 | `tests/unit/test_distribution.cpp` | 559 | **0**(连既有的 `freestanding` 短路也没有测试) |

⚠️ **这一条比它看起来重要**:本轮在三台宿主上一共修了九处缺陷,而其中至少三处
是**纯函数的错误答案** —— 产物格式判据匹配不到 `aarch64-macos`、契约在运行时已在
对象里时仍点名一个库、`llvm_triple` 的拼法。它们各自可以用一个不到十行、不需要
编译器也不需要网络的单元测试判定,而实际是用**三台 runner 的完整交叉构建**发现的。

⇒ **修复方向不是「补测试」而是「把判据放到便宜的地方」**:一个在一秒内失败的
断言,和一个在四十分钟后失败的断言,发现的是同一个缺陷,而前者会被更早地跑到。

### 2.2 ⭐ C(CI 覆盖):`openkal-opensbi` 没有 conformance 工作流

四个平台实现包里,三个跑规范的 conformance 套件,裸机那个不跑:

| 包 | conformance 工作流 |
|---|---|
| openkal-linux | ✅ |
| openkal-macos | ✅ |
| openkal-windows | ✅ |
| **openkal-opensbi** | **无** |

⚠️ 而 openkal 的 §9 把 conformance 定为**规范的核验程序**,不是可选项。裸机的
困难是真实的(需要 qemu + 固件),但 `openkal-llvm-runtime` 的 CI 已经在 qemu 上
跑裸机程序,所以困难不在于「做不到」。

### 2.3 J(文风):`假绿` 十处,本次引入

| 文件 | 处数 |
|---|---|
| `.agents/docs/2026-08-22-ecosystem-closure-design.md` | 6 |
| `.agents/docs/2026-08-23-nxn-cross-build-scheme.md` | 2 |
| `.agents/docs/2026-08-23-universal-cross-build-design.md` | 2 |

`main` 上不存在这个词 ⇒ 它是本轮引入的口语缩略,不是既有术语。它指代的概念是
**「一次报告成功而实际没有核验任何东西的检查」**,而这个概念在本文档里出现频繁,
值得有一个陈述句式的说法。

⚠️ 英文注释一侧扫描无发现(gotcha / hacky / obviously / just works 等零命中)。

### 2.4 B(稳定性):构建产物再次出现在改动里,而上一次的扫描判据自己有缺陷

`openkal-llvm-runtime/examples/import-std/.mcpp/.xlings.json` 仍在 PR 中。

⚠️ **上一轮已经做过一次这个检查并报告「六个仓干净」**,而它漏掉了这一个 ——
判据写成了 `^(\.mcpp|target/…)`,**锚定在路径开头**,所以只看得见仓库根下的那一个。

⇒ 这是一条关于 review 本身的发现:**一个检查报告「干净」时,要先问它看得见多少**。

### 2.5 待展开

第一轮的其余维度(A/D/E/F/G/H)在下面逐仓展开。

---

## 3. 修复清单

| # | 维度 | 事项 | 状态 |
|---|---|---|---|
| 01 | B | 删 `examples/import-std/.mcpp/`,并把扫描判据改成不锚定路径开头 | ✅ |
| 02 | I | `Triple::llvm_triple()` 单元测试(六种拼法 + macOS 版本号 + os 字段) | ✅ 注入 `arm64→aarch64` 验证会红 |
| 03 | I | `graph_runtime_compile_flags()` 单元测试(PE / Mach-O / ELF / 载荷伺候 / 无法解析 五态) | ✅ 注入 `os=="macos"→false` 验证会红 |
| 04 | I | `distribution` 的 `graphCxxRuntime` 与 `freestanding` 短路单元测试 | ✅ 注入去掉短路验证会红 |
| 05 | I | e2e 267:`MCPP_TARGET_REQUESTED` 的契约(本机为空 / 指名时携带) | ✅ 注入「空则填宿主」验证会红 |
| 06 | C | `openkal-opensbi` 的 conformance 工作流 | ✅ 12 held / 0 failed / 9 not observed |
| 07 | J | `假绿` 等口语改为陈述句式 | ✅ 三份文档、十二处 |
| 08 | F | std 模块第二条命令收到十九个用不上的 flag(一个字符串承载两个事实) | ✅ 产物逐字节相同为证 |
| 09 | A | 板子的加载地址被每个程序抄一份 ⇒ 归 `openkal-opensbi/board.ld` | ✅ 两个例子都删掉了自己那份;四目标复验 |
| 10 | H | `dist::format_for` 从 lambda 提成函数,兜底成为参数 | ✅ 三组断言,注入验证会红 |
| 11 | B | e2e 267 选的目标在 macOS 上无载荷(我写的是假设不是实测) | ✅ 改用宿主自己的三元组 |
| 12 | F | Windows 分支把「保留模块名」抑制绑在 `.ixx` 上 ⇒ 每次构建一条警告 | ✅ 改为无条件,与另一分支一致 |
| 13 | B | Windows 宿主 + clang 无 include 依赖追踪 | ⏸ 记录不修,见 §4.4(爆炸半径:CI 不受影响,增量开发受影响) |

---

## 4. 第一轮的其余发现(展开)

### 4.1 ⭐⭐ A(架构):板子的加载地址被每个程序抄一份

OpenSBI 把控制权交到 `0x80200000`,而在此之前**每个想在这块板子上跑的程序都自带
一份链接脚本**说出这件事:`openkal-opensbi/examples/hello` 一份、
`openkal-llvm-runtime/examples/same-source` 一份,而规范的 conformance 套件会需要
第三份。

⚠️ 这个地址不是任何程序的性质 —— 它是**这段固件把控制权交到哪里**,正是
`openkal-opensbi` 存在的理由。⇒ 归本包,由 `build.mcpp` 的 `link_script` 送到使用者
的链接线上。

⚠️ 而尺寸是这份文件里唯一的判断:打印一个字符串的 C 程序不需要 256 KiB 栈和
16 MiB 堆,带着 C 库和 C++ 标准库的程序需要 —— 并且它在**自己的初始化期间**就会耗尽
更小的数值,那发生在 `main` 之前,所以看起来是「程序启动了但什么都没打印」。

### 4.2 ⭐ F(简洁):一个字符串承载了两个事实

`stdModuleFlags` 同时携带「给哪台机器」和「头文件在哪」。构建 `std` 模块有两步,
只有第一步两样都要 —— 第二步编译的是 BMI,而 BMI 已经包含头文件贡献的一切。

⚠️ 后果不是错误而是噪声,且是**遮蔽性的**噪声:十九条「参数未被使用」的警告排在
任何有意义的警告之前。而它们在每个平台上都存在、在每个平台上都看不见 —— 非 Windows
的命令以 `2>&1` 结尾,而 mcpp 丢弃成功命令的输出。

⭐ 核验用的是产物而不是「编过了」:同一个 codegen 步骤,用拆分后的 flag 和用完整
flag 各跑一次,`std.o` **逐字节相同**(sha256 `a6d837241e7f02b2`,736 字节)。

### 4.3 ⚠️⚠️ 一条被写进本文档、随后被实测推翻的发现

**原文**:「`same-source` 在 `openkal-opensbi` 开始供给链接脚本之后,链接线上仍然
只有一个 `-T` —— 它自己那份 ⇒ 没有回归,而且这个机制不是可传递的。」

**那是错的,两句都错。** 那次观察发生在推送传播到 git 缓存**之前**,量到的是旧状态。
真实情况:板级事实**可传递**地到达使用者,于是 `same-source` 拿到两份脚本、两份都被
应用:

```
ld.lld: error: section .eh_frame file range overlaps with .debug_str_offsets
ld.lld: error: section .debug_str file range overlaps with .eh_frame_hdr
```

⚠️ 而诊断一个字都没提「有两份」。

⇒ 修复是把 `same-source` 自己那份删掉,那本来就是预期的终态:板级事实只有一份来源。
四个目标复验全部产出,三个能在本机跑的输出逐字相同。

⭐ **这条留在文档里而不是被改写,因为它是本次 review 唯一一条自己出错的记录**,而
它出错的方式值得一记:**在一个依赖会被缓存的系统里,「我改完之后量了一下」不等于
「量的是改完之后的状态」**。上一轮已经因为同一个原因给出过一次错误结论(改 path
依赖的 manifest 不让消费者的 plan 失效)。

### 4.4 ⚠️ B(稳定性):Windows 宿主上没有 include 依赖追踪 —— 记录而不修

mcpp 自己在这个组合上打印一条诚实的诊断:

> this toolchain and platform combination emits no GNU depfile
> impact: editing a file `#include`'d inside a module interface purview … will
> not trigger a rebuild, so the build may reuse a stale BMI or object

原因写在 `ninja_backend.cppm` 里:GNU 的 depfile 过滤需要 `awk`,Windows 上没有;
`cl.exe` 走 `deps = msvc` 不受影响,受影响的是 **Windows 宿主 + clang/mingw**。

⚠️ **爆炸半径**:CI 每次都是干净检出,不受影响;受影响的是 Windows 上的**增量开发
构建**。而本轮新加的 3×3 工作流恰好把这个组合变成了常规路径,所以它从「一个少见的
组合」变成了「一台宿主的默认状态」。

⇒ **不在本轮修**:修它需要一条不依赖 `awk` 的 depfile 过滤,那是它自己的一次改动。
记在此处,连同它的爆炸半径,因为一条诊断如果没人记下它影响什么,下一个人只会把它
当噪声关掉。

### 4.5 ⭐ 同一个函数里的三处不对称,而它们是被逐个撞出来的

`clang.cppm` 的 `std` 模块命令有一条 `#if defined(_WIN32)` 分支,本轮在它里面找到
三处缺陷,每一处都要一次完整的 CI 轮次:

| # | 症状 | 缺的是什么 |
|---|---|---|
| 1 | `std.cppm:16: '__config' file not found`(命令只有五个 token) | `extraFlags` —— 包自己的头、`-nostdinc`、目标三元组 |
| 2 | `module file 'pcm.cache\std.pcm' not found` | 绝对路径(`cd X && …` 在 cmd.exe 不换盘符) |
| 3 | `'std' is a reserved name for a module` | `-Wno-reserved-module-identifier`(被绑在 `.ixx` 上) |

⚠️ 三处的共同原因是同一个:**这条分支写在「Windows 宿主只为自己构建、对着 MSVC
STL」的年代**,而它此后一直没有被那个假设之外的输入跑过。

⇒ 一个只在一种平台上编译的分支,是这台机器无法检查的分支。本轮在 `std.compat` 那侧
写了这样一条分支又撤回了,改成两边共用的单一形式;`std` 这条还在,因为它有真实的
理由(cmd.exe 会剥掉前导引号)。⚠️ **那个理由留下了一处只能靠 CI 检查的代码**,这一
条记在这里而不是当作已经解决。

### 4.6 ⭐ H(扩展性):加第五个平台要动三处,而它们都是表

本轮引入的按 os 分支的判据集中在三个函数里:

| 函数 | 分支数 | 现在有测试吗 |
|---|---|---|
| `dist::format_for` | 3 | ✅(本轮提出成函数后新增) |
| `graph_runtime_compile_flags` | 2 | ✅ |
| `Triple::llvm_triple` | 4 | ✅ |

⚠️ 而 `format_for` 此前**不是函数**,是一个一千五百行函数里的 lambda —— 那正是它
没有测试的原因,也是它答错时只能靠三台宿主跑出来的原因。提出来之后,「宿主兜底」
成为一个参数,于是这个判据可以被检查而不必成为它所描述的那台机器。

---

## 5. 第二轮

(第一轮修复完成后填写)

---

## 6. 判据

1. 两轮 review 各自留下**具体的、可复核的**发现,而不是「看过了」。
2. 每一条发现或者被修复,或者写明为什么不修。
3. 八个 PR 的 CI 全绿,且第一轮新增的测试**能够失败** —— 每个新测试都以「注入
   缺陷 → 断言变红」核验过。
