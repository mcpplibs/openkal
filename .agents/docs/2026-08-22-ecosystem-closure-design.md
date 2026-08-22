# openkal 生态闭环设计方案:C++ 运行时、交叉构建与原生内核方向

**状态**:设计,未实施。本文是一次完整讨论的沉淀,尚未有任何一条被执行。
八项设计取舍已经过 review 并定案,记录在 §13。

**证据来源分两类**:标注为「实测」的结论在本机验证过,工具版本随文注明;
其余为设计主张。这个区分在本文里承重 —— 全篇只有八条是实测,其余都是推理,
而本项目账本上「实测推翻推理」出现过不止一次。

**本文记录推导过程,包含被撤回的方案及其理由**;规范正文(`SPEC.md`)只记录结论。
两者冲突时以规范正文为准。

配套:`mcpp` 仓库 `.agents/docs/2026-08-20-openkal-design.md`(0.4 的设计推导)、
`2026-08-20-openkal-portable-program-findings.md`(0.4 的两条规范条款)。

---

## 0. 摘要与判断

这份方案覆盖三件事,它们的瓶颈类型完全不同,**不该被当成一条线排优先级**:

| | 内容 | 瓶颈 | 风险类型 | 能否估量 |
|---|---|---|---|---|
| **I** | 兑现现有分层:C++ 运行时上到 openkal、交叉建三个 OS、嵌入式 C++23 | 工程量 | 技术,已知 | ✅ |
| **II** | 规范补齐:profile、能力三态、可执行内存、每上下文指针槽 | 设计,不可逆 | 设计 | ❌ |
| **III** | 制品可移植(统一格式 / 安装期物化) | 输入尚未定义 | 依赖 I+II | ❌ |

**核心判断三条:**

1. **II 是 I 和 III 的上游,而且它零成本、不可逆、没有实验能回答它。**
   它应当先于一切工程被决定。
2. **I 的主线是「C++ 运行时上到 openkal」一件事**,它同时是 `import std`、
   嵌入式 C++23、C++ 交叉、以及 III 的「一份代码段」的共同前提。
3. **III 应当暂缓**,不是因为难,是因为它的输入(原生镜像格式)取决于 II。

**要适配的是 LLVM 的运行时库,不是编译器。** 见 §6。

---

## 1. 目标与非目标

### 目标

- **G1** 一份 C++ 标准库,配置在 openkal 之上,使 `import std` 与异常在 openkal
  的全部环境上可用 —— **包括裸机**。
- **G2** 一台主机交叉构建 Linux / macOS / Windows 三个目标,**不需要 Apple SDK,
  不需要 MSVC SDK**。
- **G3** 规范补齐 profile、能力三态与两条原语,使其具备被原生内核实现的表达能力。
- **G4** 全部工程用 mcpp 构建,包括 fork 的上游 LLVM 运行时。
- **G5** 深嵌入式(Cortex-M / RV32 MCU 级)纳入范围,形态为 `openkal-picolibc`。
  见 §8.3。

### 非目标

- **N1 不追求「一个二进制在多个 OS 上启动」**。那是 III,而且 Cosmopolitan/APE
  已经把这条路走了很多年。openkal 的价值主张是**源码写一次**,不是二进制发一份。
- **N2 不 fork 编译器**。clang 与 lld 一行不改,理由见 §6.1。
- **N3 不支持 sanitizer**。compiler-rt 的 sanitizer 大量依赖宿主内部。
  这是一条真实缺口,应当写进文档而不是留给使用者发现。
- **N4 本轮不做虚拟化闭环**。见 §9.2 备注。

---

## 2. 判据

本方案所有取舍都出自下面四条。前三条是从 openkal 现有推理里提炼的,第 0 条是
review 中明确的前提。

### 2.0 ⭐ 前提:openkal 不依赖任何 backend

**openkal 的接口内容,由「一个通用内核接口该有什么」决定,不由任何 backend 决定**
—— 既不是三个宿主的交集,也不是某个未来原生内核的需要。它是通用 ABI / SPEC 层,
backend 实现它们有理由实现的部分。

这一条否掉了一个曾被认真考虑的框架(「规范该按宿主交集设计还是按原生实现的表达
需要设计」):**两个都不是**。规范按自身的完整性设计,取舍依据是下面三问,
而不是任何一份实现的现状。

**直接后果**:`mprotect` / `pipe` / `poll` / signal 目前不在接口里,**不是因为
「三个宿主的交集不含它们」**(事实上三个宿主都有),而是它们尚未被规范定义 ——
即 §2.3 意义上的 `unspecified`。要不要加,依据是「一个通用内核接口该不该有」,
与谁能实现无关。

### 2.1 分层三问(按顺序问)

**Q1 它跨不跨越 openkal 那条边界?**
不跨越 ⇒ 任何规范都不该有它。
> 例:unwind 表注册。`.eh_frame` 的地址与长度是**镜像的属性**,链接脚本给得出;
> 而 openkal 里没有任何东西会展开栈(无信号、无异步投递、实现全部
> `-fno-exceptions`)—— **异常永远不穿过 openkal 边界**。

**Q2 它随架构变,还是随环境变?**
随 arch ⇒ openarch;随环境 ⇒ openkal。
> `task.h` 已经用过这个句式:*「The register convention that delivers this belongs
> to openarch rather than to openkal」*。⚠️ 但要注意这句话说的是**机制**;
> 一个接口对**自身**调用约定的说明不是机制,不适用这一条。见 §5.5。

**Q3 每个环境的答案是不是相同?**
相同 ⇒ 放在 openkal **之上**,共享一份。
> 这正是 clause 7.1 把描述符表与名字解析排除在实现之外的理由:
> *「a table required of every implementation is a compatibility layer written as
> many times as there are environments」*。

### 2.2 core 最小 + 可选能力 + 命名束

**定案:`core` 保持最小,为的是支持裸机与无 OS 环境;core 之上的一切统一作为
可选 / 配置能力。**

```
core   = abort + stream + memory        ← 强制,所有实现必须有
其余    = 可选能力(fs / process / env / time / event / 可执行内存 / 内存保护 …)
```

`core` 这个概念已经存在(设计笔记里的「core = abort + stream + memory」),
本方案只是把它提升为接口里的一等物,并明确它**不再增长**。

**判别式(已有,继续沿用)**:假实现会让上层静默地错 ⇒ 模拟,不该有;
只是容量小 / 会失败 ⇒ 实现。静态 arena 上的 bump allocator 是实现 ⇒ memory 进 core。

**命名束(profile)是生态的表达单位,不是合规等级。** 能力位回答「你有没有 X」,
一次一个,它撑不起两件事:

- 程序没法针对任意子集写 —— 2^N 的组合空间
- 生态没法表达「这个包需要什么等级的环境」

所以在 core 之上定义**命名的可选能力束**,供清单声明依赖用 —— 但它们
**不构成对实现的强制要求**:一个实现声明它提供哪些能力,而不是声明它符合哪一级。

先例一致:POSIX 有 option group 与 PSE51/52/53/54;SBI 有 extension +
`sbi_probe_extension`;UEFI 有 required/optional protocol —— 三者的 required 部分
都很小,可选部分才是主体。

**时序约束**:命名束与三态机制必须在第二个原生实现出现之前立起来。事后追加等于
重新划分已有实现的合规边界。现在五个实现全在一处,是成本最低的时刻。

⭐ 顺带解掉一个别扭:**原生镜像格式是一个可选能力**,适配实现不提供它,
因此既不需要实现,也不需要一个特设的位去报告「我用宿主格式」—— 它只是没有那条能力。
上层的物化器查能力。

### 2.3 能力三态

现在一条能力不可用只有两种表达(位为 0,或位不存在),而它掩盖了三种情况:

| 状态 | 含义 | 上层该怎么办 |
|---|---|---|
| **provided** | 实现提供了 | 用 |
| **declined** | 规范定义了,这个实现有理由不提供 | **写降级路径** |
| **unspecified** | **规范还没定义** | **不该写降级路径** |

后两者对上层的表现完全一样(一个 `ENOSYS`),但一个是「这里永远不会有」,
另一个是「这里还没有」。

⚠️ 这一条不是理论洁癖。本项目已经吃过同型的亏:一次真因是**没做完的迁移**,
而上层为它写的降级分支**从未执行过**。`openkal.event` 现在是
"reserved and unspecified" —— 说明这个区别已经被意识到,只是写在文档里
而不在接口里。

**定案:加一个查询接口**,而不是把三态挤进现有的 props 位。见 §5.2。

---

## 3. 架构

```
                     程序(普通 C / C++ 源码,不提这套东西一个字)
                          │
   ┌──────────────────────┴──────────────────────────────────┐
   │  openkal-llvm-runtime   libc++ / libc++abi / libunwind / builtins │  ← 新增
   │  openkal-musl           musl 1.2.5,1345 源文件原样编译             │
   │  openkal-picolibc       深嵌入式的 C 库(§8.3)                     │  ← 新增
   └──────────────────────┬──────────────────────────────────┘
                          │
              openkal(48 个函数 + core + 可选能力 + 三态查询)
                          │
   ┌──────┬──────┬────────┼────────┬──────────┬──────────┐
 linux  macos  windows   uefi   opensbi   (未来:原生内核)
   └───────── 适配型实现 ─────────┘        └── 原生型实现 ──┘
```

**两类实现,对格式的关系相反:**

| | 适配型 | 原生型 |
|---|---|---|
| 格式由谁决定 | 宿主早就定了,实现无权更改 | **实现自己定** |
| 加载的负担 | 委托宿主 `exec` —— 一行 | 是它作为内核的本职 |
| 对「原生镜像格式」能力 | 不提供 | 提供 |

因此 `kal_process_spawn` 的形状**不变**(接受名字而非字节):适配实现转发给宿主,
原生实现加载原生镜像格式。**「从字节加载」这个操作不进规范** —— 它会迫使适配
实现自己写一个宿主不认识的格式的加载器,那正是 Q3 要消灭的 N 遍兼容层。

**为什么 48 个函数这个规模是关键**:让一个抽象层到处可用只有两种办法 ——
模拟它,或者缩小它。Linux 用户态 ABI 是 ~360 个 syscall,大到只能模拟,
而模拟它需要一个内核,所以只能是虚拟机。openkal 是 48 个函数(**实测**:
fs 18、stream 7、task 6、env 5、process 4、time 4、abort 2、memory 2),
小到每个环境可以**原生**实现。**这个比值就是「要不要 VM」的分界线**,
也是 §9.2 不做虚拟化的根据。

---

## 4. 仓库与分支布局

### 4.1 涉及的仓库

| 仓库 | 角色 | 本方案的改动 |
|---|---|---|
| `mcpplibs/openkal` | 规范 | core 定型、三态查询、可执行内存、原生镜像格式能力 |
| `mcpplibs/openarch` | 架构机制层 | 每上下文指针槽(与 `arch_cpu_percpu` 平行)|
| `mcpplibs/openkal-linux` | 适配实现 | 三态 + 能力声明;TLS 槽 |
| `mcpplibs/openkal-macos` | 适配实现 | 同上;自写 libSystem stub |
| `mcpplibs/openkal-windows` | 适配实现 | 同上;自写 `.def` + 原型;SysV thunk |
| `mcpplibs/openkal-uefi` | 适配实现 | 同上 |
| `mcpplibs/openkal-opensbi` | 适配实现 | 同上 |
| `mcpplibs/openkal-musl` | C 库 | Mach-O 15 个符号;`__register_frame`;emutls 底座 |
| **`mcpplibs/openkal-llvm-runtime`** | **C++ 运行时(新建)** | 见 §6 |
| **`mcpplibs/openkal-picolibc`** | **深嵌入式 C 库(新建)** | 见 §8.3 |
| `mcpplibs/std-freestanding` | 裸机子集 | 与 `import std` 的并存关系 |
| `mcpplibs/sbase` | 验证 | 97 个工具,交叉构建的判据 |
| `mcpp-community/mcpp` | 构建工具 | libc++ 的 std 模块、能力解析、交叉目标 |

### 4.2 分支约定

**定案:全部仓库使用同一个分支名 `feat/openkal-closure`。**

统一分支名的理由是它让 git 依赖的引用是机械的:每一处都是同一个字符串,
不需要为每个仓库记不同的分支。一次「切到这套东西的开发线」等于把所有
`branch = "feat/openkal-closure"` 一起解析到位。

⚠️ 代价要写清楚:**统一分支名意味着任何一个仓库把这条分支推坏,所有依赖它的
构建同时红。** 这是有意的 —— 它让「这条线整体是否可构建」成为一个可观察的事实,
而不是分散在十二个仓库里。

### 4.3 依赖引用方式

**实测(读 `mcpp/src/manifest/toml.cppm:715,780`)**:mcpp 已支持 git 依赖,
`git = <url>` 必须搭配 `rev` / `tag` / `branch` 三者之一,否则解析期报错。
本方案不需要为此改动 mcpp。

```toml
[dependencies]
openkal = { git = "https://github.com/mcpplibs/openkal", branch = "feat/openkal-closure" }

[target.'cfg(os = "linux")'.dependencies]
openkal-linux = { git = "https://github.com/mcpplibs/openkal-linux",
                  branch = "feat/openkal-closure", features = ["standalone"] }
```

**约定**:开发验证期一律 git + branch;进入发布流程时统一换回版本号 + 索引。
两种形态不混用 —— 一个清单里既有 git 又有 version 的依赖,会让「这次构建到底
用的哪一份」变成要逐条查的事。

### 4.4 构建工具

**全部仓库用 mcpp 构建,包括 fork 的上游 LLVM 运行时。** 这不是形式要求,
它是一条判据:如果 mcpp 建不了 libc++,那说明 mcpp 的构建模型对真实世界的
大型 C++ 代码库还不够;把这件事暴露出来比绕过去有价值。

---

## 5. openkal 规范侧改动

### 5.1 core 定型 + 可选能力

见 §2.2。规范需要:

- 明确 `core` 的内容与它**不再增长**这一性质
- core 之上的能力统一为可选,每个实现在一处声明它提供哪些
- 定义若干**命名束**供上层清单声明依赖,**不匹配在解析期报错而非链接期**

mcpp 侧的机制已经现成:`docs/13-baremetal.md` 里 `freestanding-allocator`
那套 capability provider(多个提供者在解析期报错)是同一形状。

### 5.2 ⚠️ 能力三态:读了 SPEC 之后撤回「查询接口」

**review 定案是「加一个查询接口」。实现前读 `SPEC.md` §6,发现该方案与规范
现有设计正面冲突,且规范已经用更好的方式解决了同一个问题。据此撤回。**

#### 规范早已区分三态,而且每一态都在最早可知的时刻被发现

| 状态 | 现有机制 | 何时发现 | 规范条款 |
|---|---|---|---|
| **provided** | 符号存在 | 链接通过 | §6.1 |
| **declined** | 符号缺失 | **链接失败** | §6.1 / §6.2 |
| **unspecified** | 模块不存在 | **编译失败** | §6.1 |

§6.1 原文:*「An interface that an implementation does not provide is absent as a
link-time definition, and a consumer that uses it fails to link… An interface
that the specification does not define is absent as a module, and a consumer
that imports it fails to compile.」*

而 §6.2 已经把这件事画成一张表(dependency resolution / link / run),
并注明每一行是「the earliest at which it exists」。

#### 运行期查询会是规范明令的缺陷

§6.1 同一段:*「A conforming implementation shall not provide an interface whose
operations report a lack of support at run time; the specification treats
**run-time refusal as a defect** and not as a means of expressing partiality.」*

一个返回 `declined` 的运行期查询,正是把编译期/链接期的事实降级到运行期 ——
它要求每个调用方写一条降级路径,而**大多数制品永远不走那条路径**。
⚠️ 这恰好是本项目已经吃过的那个亏的形状:降级分支从未执行过。

#### 我最初识别的缺口是真的,但它不在 openkal

「`declined` 和 `unspecified` 对上层长得一样,都是 `ENOSYS`」这个观察成立,
但**主语错了** —— 那是 **openkal-musl** 的行为,不是 openkal 的。
openkal-musl 是 POSIX 兼容层,POSIX 要求运行期 `ENOSYS`,所以它必须压平。

⇒ **修复位置在 openkal-musl:它可以在构建期(依赖解析)区分两者** ——
「openkal 实现没提供这条接口」与「规范还没定义这条接口」在它的构建配置里
是两件不同的事,应当分别记录并在文档中分别陈述,而不是共用一个 `ENOSYS` 注释。

#### 结论

- **openkal 侧:不加查询接口。** 改为把这条已有的三态判别**明确写进规范文本**
  —— 它现在分散在 §6.1 与 §6.2 里,没有被当成一个整体命名过。
- **openkal-musl 侧:** 在 `README.md` 的「什么不可用」表里,把
  「规范未定义」与「实现未提供」分成两栏。

⭐ 这一条也给判据加了一句:**在给一个规范加东西之前,先确认它没有已经用别的
方式解决同一个问题。** 我提出查询接口时没有读 §6,而 §6 正是关于这个问题的。

### 5.3 可执行内存

**这是本方案建议加进 openkal 的两条新原语之一。**

判据(§2.1)全过:跨越边界 ✅;随环境变 ✅(`mmap(PROT_EXEC)` / `VirtualAlloc` /
`MAP_JIT` / 裸机直接就是);各环境答案不同 ✅。

**理由不是「加载程序」,是 JIT。** 任何在运行期生成并执行代码的东西
(脚本引擎、正则的编译模式、shader 编译器)在 openkal 之上**今天完全写不出来**
—— 不是难,是没有任何办法拿到一块能执行的内存。加载器只是这个原语的一个用户,
而且不是最好的那个。

这个拆分还有一个好处:作为「可执行内存」它是一条边界清楚的操作;
作为「加载」它就成了一个时灵时不灵的核心操作。

#### ⚠️ 形态修正:是一个独立接口,不是能力位

我最初写的是「能力位语义很自然(macOS ⇒ declined)」。读 SPEC §6.2 后修正:

> *An **operation** an implementation may lack becomes an interface of its own,
> so that its absence is reported by the linker. A **property** that varies
> between implementations is reported by a capability word.*

可执行内存是**操作**不是属性 ⇒ 它必须是**自己的接口**(`openkal.exec`),
缺失由链接器报。用能力位表达会把一个链接期事实推到运行期,
而 §6.1 把「提供了却总是失败」直接判为缺陷。

#### ⭐ 但 macOS 暴露了规范没有覆盖的第三种偏性 —— 已补为 §6.5

macOS 上能不能拿到可执行内存,**既不取决于实现,也不取决于资源**,而取决于
**制品是怎么产出的**:`MAP_JIT` 要 `com.apple.security.cs.allow-jit`
entitlement,而 entitlement 是在**链接之后**由签名步骤加上的。

- 不是 §6.2 的「实现缺这个操作」—— 实现有,只是得配合签名
- 不是 §6.4 的「某些资源永远满足不了」—— 同一程序里每块区域行为一致

⇒ **规范新增 §6.5「Availability settled by how the artifact is produced」**:
这类接口在**依赖解析期**提供或撤下(§6.2 那张表里最早的一行),
由实现包的一个 feature 表达。要求它的程序既拿到接口、也拿到一份按需签名的制品;
不要求的程序两样都没有,用了就链接失败 —— 回到 §6.1 的常规报告。

规范里落下的通则一句:**一条操作的可用性若由制品如何产出决定,就不在运行期报告
—— 运行期报告会让每个调用方带一条绝大多数制品永不经过的路径,
而没有制品经过的路径是没有东西验证过的路径。**

#### 已实现

`include/openkal/exec.h` + `src/exec.cppm` + `SURFACE.txt` 四个名字 +
`SPEC.md` §3 清单行、§4 表格行、§6.5 新条款。三个操作而非一个,因为
**目标环境现在全都区分「可写」与「可执行」**:区域先可写,`kal_exec_publish`
之后可执行,**从不同时**。一个返回二者兼备的内存的接口在其中两个环境上
无法实现。

`tools/check-declarations.sh` 抓到了新名字未进一致性文件(56 个实体),
`--write` 回填后复查通过 —— **判据在这次改动上真的红过一次。**

### 5.4 原生镜像格式

**是一个可选能力,只有原生型实现提供。**

理由:两个原生 openkal 内核如果各定各的格式,就是在一个新层上重演当初促成
openkal 的那件事 —— **而且这次是自己造的碎片,不是继承的**。适配实现不提供
这条能力,因此不受影响。

### 5.5 ⭐ openarch 调研结果(本轮新增)

**调研结论:openarch 接得住指针槽,但接不住调用约定 —— 而且我先前把调用约定
推给 openarch 是错的。**

**实测(读 `openarch/abi/include/openarch/abi.h`,openarch 0.6.0)**:
openarch 自述为 *"the architecture-mechanism layer — execution contexts, traps,
per-CPU state and address spaces, as one interface over several instruction sets"*,
接口面包括:

```c
void  arch_context_switch(void* from, void* to);
void  arch_context_init(void* ctx, void (*entry)(void*), void* arg, void* stack_top);
void* arch_cpu_percpu(void);          /* ← 关键 */
void  arch_cpu_set_percpu(void* p);   /* ← 关键 */
void  arch_cpu_fence(int barrier);
```

**① 指针槽:形状先例已经存在,但不能直接复用。**

`arch_cpu_percpu` / `arch_cpu_set_percpu` 证明「一个指针槽 + get/set」这个抽象
在 openarch 里是**已被接受的形态** —— 这条正是我们需要的形状。

⚠️ 但它是 **per-CPU 不是 per-context**,而三个架构上这两者**用的是不同的寄存器**:

| | 每 CPU(内核态) | 每线程(用户态) |
|---|---|---|
| x86_64 | `%gs` | `%fs` |
| aarch64 | `TPIDR_EL1` | `TPIDR_EL0` |
| riscv | 监督态的 `tp` 惯例 | 用户态 `tp` |

⇒ **需要一条与 `arch_cpu_percpu` 平行的每上下文槽原语,而不是复用它。**
好消息是 `arch_context_init(ctx, entry, arg, stack_top)` 已经存在 ——
**上下文初始化的位置是现成的**,加一个槽参数或一条平行的 set 是自然的延伸。

**② 调用约定:留在 openkal,不推给 openarch。**

我先前的判断(推给 openarch)依据的是 `task.h` 里那句 "belongs to openarch"。
调研后发现那句说的是**机制**(传递 TLS 的寄存器约定),而 openarch 的整个接口面
确实全是**机制**(上下文切换、陷入、PTE 编码、屏障)—— 里面没有任何一条是
「约定的文字说明」。

而「`kal_*` 按该架构的标准 psABI 调用,与宿主 OS 无关」是 **openkal 对自身接口
形状的自我描述**,不是一条架构机制。一个接口规范不说明自己的调用约定就是不完整的,
这和它必须说明参数顺序、struct 布局是同一类。

⇒ **修正:调用约定进 openkal(自我描述),指针槽进 openarch(机制)。**

实现负担仍然落在 openkal-windows(SysV ↔ MS x64 的 thunk),位置正确 ——
只有那个环境知道怎么桥。

### 5.6 ⭐ TLS 锚点:一次依赖倒置

**现状是一个薄弱点,而且它独立于本方案的其余部分就该修。**

`task.h` 现在的立场是:*「observes the thread-local storage of **the toolchain that
compiled the program**」*,并以 `KAL_TASK_PROP_THREAD_LOCAL` **报告**而不提供。
注释自己写了后果:*「cannot be ported onto an implementation that lacks the property」*。

也就是说 **openkal 是 TLS 的消费者而不是提供者**,它自己的每上下文状态存在一个
`thread_local` 里 —— 依赖一个它控制不了、按 OS 工具链变化、而且可选的东西。

**实测(clang 18,本机)**:同一份 `thread_local int x`,三个 target 出来是三段
完全不同的指令 ——

```
Linux:    movl %fs:x@TPOFF, %eax
macOS:    adrp x0, _x@TLVPPAGE          ← TLV 描述符,一次间接调用
Windows:  movl _tls_index(%rip), %eax ; movq %gs:88, %rcx
```

**同一个 arch,三个答案。** 这不是 flag 差异,是编译器按 triple 选的三套机制。

**提议**:把 `KAL_TASK_PROP_THREAD_LOCAL`(可选、要求完整工具链 TLS)换成一条
更弱、更基础、且**属于 core** 的保证 —— **每个上下文有一个可用的指针槽,
在上下文创建时被初始化**(机制由 openarch 提供,见 §5.5)。

- 更弱的性质更容易被满足 —— **裸机也给得起**,而完整的工具链 TLS 给不起
- 键值表、emutls、`errno` 定位全部落到 openkal-musl(Q3:各环境答案相同)
- ⇒ **扩大**了 openkal-musl 的可移植范围,是净收益

⚠️ 代价:openkal 自己得**停止使用 `thread_local`**,改用新提供的槽 ——
否则依赖没倒过来,只是多了一条接口。这要动五个实现,不是一句规范。

### 5.7 撤回的方案及理由

| 方案 | 撤回理由 |
|---|---|
| `kal_process_load(bytes)` | 迫使**适配实现**自己写一个宿主不认识的格式的加载器 ⇒ N 遍兼容层(Q3)。而且 macOS 要 JIT entitlement、裸机要从头写加载器 |
| openkal 定义一个所有环境通用的镜像格式 | 适配实现的格式由宿主决定,规范无权更改。正确形态是一条只有原生实现提供的可选能力 |
| 「规范该按宿主交集还是按原生需要设计」这个二选一 | 前提错误。openkal 不依赖任何 backend,按自身完整性设计(§2.0) |
| 把调用约定推给 openarch | 调研后否掉。openarch 全是机制,没有「约定的文字说明」;调用约定是 openkal 对自身的描述(§5.5) |
| 给 clang 加一个新目标格式 | 见 §6.1。这是整条路上唯一真正昂贵、而又完全可以跳过的动作 |
| fork 并 patch lld 以支持 N_INDR | 见 §7.1。修 port 层便宜两个数量级,且**产物用 stock lld 就能链** —— 一个只有自家链接器能链的包,别人没法复核 |
| 虚拟化闭环构建期 | 本轮不做,见 §9.2 |

---

## 6. openkal-llvm-runtime

### 6.1 范围:runtimes 而非 tools

LLVM 仓库里装着两类成本差一个数量级的东西,而 LLVM 自己就是这么分的:

| | 目录 | 是什么 | 本方案 |
|---|---|---|---|
| **tools** | `llvm/` `clang/` `lld/` | 编译器、链接器 | ❌ **一行不改,也不 vendored** |
| **runtimes** | `compiler-rt/` `libunwind/` `libcxxabi/` `libcxx/` | 跟着程序进镜像的库 | ✅ **全部工作在这里** |

**实测(本机 stock Ubuntu clang 18)**:同一个 clang 二进制直接产出三种格式的对象 ——

```
--target=arm64-apple-macos14    → Mach-O 64-bit arm64 object
--target=x86_64-pc-windows-msvc → Intel amd64 COFF
--target=x86_64-w64-mingw32     → Intel amd64 COFF
```

编译器对 openkal 一无所知也不需要知道:它按 `--target` 生成代码,而 openkal
不改变任何 arch 或格式。

**关键实践后果**:runtimes 可以用一个**现成的** clang 单独构建
(LLVM 官方的 `LLVM_ENABLE_RUNTIMES` 入口就是为此存在的)。
⇒ 不需要构建 LLVM。工作量从「小时级 + GB 级的编译器构建 + 每个版本重新打载荷」
掉到「分钟级的库构建」,而且**可以当源码包做**。

### 6.2 仓库形态(openkal-musl 风格)

**定案:仓库名 `openkal-llvm-runtime`;vendoring 范围为四个 runtimes 子树,
不 fork 整个 llvm-project。**

理由:openkal-musl vendored 了整个 musl(1345 个文件),而 llvm-project 大
**两个数量级**(十万级文件、GB 级)。整树 fork 会让 clone 与 CI 成本高一个量级,
而其中 95% 以上的内容(编译器本体)本方案一行都不改 —— 按 §6.1,它们也不该改。
仓库名带 `-runtime` 后缀,让名字与内容一致。

```
openkal-llvm-runtime/
  llvm/libcxx/               vendored 上游子树,尽量原样
  llvm/libcxxabi/
  llvm/libunwind/
  llvm/compiler-rt-builtins/
  llvm/PATCHES.md            逐条记账:改了哪几行、换了哪几个文件、为什么
  port/                      桥接层:openkal 上的 unwind 注册、emutls 底座
  llvm-generated/<config>/   configure 的产物,预生成并入库
  examples/                  能失败的断言
  mcpp.toml                  per-target build 块
  README.md                  与 openkal-musl 同体例:改了什么、没改什么、
                             什么可用、什么不可用、量了什么
```

**`llvm-generated/` 是 `musl-generated/` 的直接对应物**。libc++ 需要 configure
产物(`__config_site`、module map、version 头),而 openkal-musl 已经证明了
这个模式:预生成、入库、`README.md` 记录怎么产生的。分目录维度取
(arch × 数据模型 × 能力集),与 musl 那边 `x86_64` / `x86_64-windows` /
`aarch64` 的分法同源。

⚠️ **上游同步策略要在第一次 vendored 时就定下来**(用 `git subtree` 还是脚本化的
拷贝 + 版本戳),否则第二次同步会变成手工比对。

### 6.3 四个组件的适配清单

| 组件 | 判决 | 说明 |
|---|---|---|
| clang / LLVM 后端 | ❌ 不改,不 vendored | 实测,见 §6.1 |
| **lld** | ❌ 不改,不 vendored | 前提是 §7.1 修掉 15 个 N_INDR |
| compiler-rt **builtins** | 🟡 不改,但要**为目标编一份** | ⚠️ 裸机那次的教训:builtins 的缺口是 **128 位移位**暴露的 —— 缺了不报错,跑到才炸 |
| compiler-rt **emutls** | 🟡 不改,但**依赖 musl 的 pthread key** | 见 §10.1 实验 A |
| compiler-rt **sanitizers** | ❌ **不支持** | 非目标 N3 |
| **libunwind** | ✅ 适配 | 见下 |
| **libc++abi** | ✅ 适配 | 主要随 libc++ 的配置走 |
| **libc++** | ✅ **主要工作** | 见下 |

**libc++ —— 性质是「配置」不是「移植」。** 它本来就为可移植设计:
`LIBCXX_HAS_MUSL_LIBC` 现成,`_LIBCPP_HAS_*` 一堆开关(threads / filesystem /
locale / random_device / wide characters)。工作是**为 openkal-musl 做一次
configure**,把没有的关掉。

这正好逐字对上那条判据:**判据是那份实现有没有为这个目标 configure 过,
不是头文件找不找得到。**

⭐ 因为 `core` 保持最小(§2.2),libc++ 的配置维度直接对应可选能力集:
没有 `fs` 能力 ⇒ 关掉 `filesystem` 与 `iostream`;没有 `task` ⇒ 关掉 threads。
**这让「libc++ 需要哪一级环境」不是一个额外的概念,而是能力集的函数。**

**libunwind —— 工作量小,坑在「怎么找到 `.eh_frame`」。**
它找展开表的方式逐 OS 不同:Linux 用 `dl_iterate_phdr`,macOS 用
`_dyld_find_unwind_sections`,Windows 用 SEH —— 这三条在 openkal 上一条都不成立。

⇒ 适配 = 让它走**静态注册**路径,由 openkal-musl 的 start 在启动时把链接脚本
给出的 `__eh_frame_start/end` 交进去。

⚠️ **这条的失败模式是全方案最坏的**:漏注册的表现是抛异常直接 `terminate`,
零诊断,而且编译、链接、正常路径全绿。**判据必须是「跨多层栈帧真抛一次并接住」**,
不能是「编过了」。

### 6.4 `import std`

libc++ 自带 `std.cppm`(需要 configure),这是选 libc++ 而不是 libstdc++ 的
第二个理由(第一个是 libstdc++ 焊在 GCC 构建系统里,没有独立 runtimes 入口)。

**连带的战略后果要点明**:GCC 可以用 libc++(`-nostdinc++` + 指头文件),但
**GCC 的 modules 实现 + libc++ 的 `std.cppm` 这个组合基本没人测过**,
而这条路的核心目标之一就是 `import std`。

⇒ **C++ 侧走 clang + libc++;GCC 留在它已经赢的地方**(Linux 原生、
已发布的 mingw-cross、裸机 RISC-V)。这与 §7 「交叉三个 OS 要押 clang」
是两条独立推理指向同一选择。

**mcpp 侧的对应项**:现有的 std 模块预编译路径是围绕 GCC 的
`bits/std.cc` / `--compile-std-module` 建的,需要为 libc++ 的 `std.cppm` 加一条。

---

## 7. 交叉构建三个 OS

**目标是「一台主机产出三个 OS 的三个原生可执行文件」**,不是一个二进制跑三个 OS。

### 7.1 ⭐⭐ 实测推翻本节的原方案:15 个 indirect symbol 不是交叉的阻塞

**本节原本设计了一条 `OKM_ALIAS_2` thunk 分支来消除 15 个间接符号。
实测证明那条修复不需要 —— 用开源工具链时这些符号根本不产生,
Linux → macOS 的交叉链接今天就能通。**

#### 原假设

`openkal-musl/port/include/features.h` 的 Apple 分支用

```c
__asm__(".globl _" #new "\n\t.set _" #new ", _" #old);
```

造第二个名字。`musl/PATCHES.md` 记录:当 `old` 定义在别的 TU 时产出 **N_INDR**,
而 lld 报 `TODO: support aliasing to symbols of kind 1`,故链接线上写着
`--ld-path=/usr/bin/ld` —— 一个只有 macOS 上才存在的文件。据此推断 macOS 目标
只能在 macOS 主机上建。

#### 实测(2026-08-22,本机 Linux)

| # | 测什么 | 结果 |
|---|---|---|
| 1 | 把 musl 全量源码(1336 TU)为 `arm64-apple-macos14` 编译,`llvm-nm -m` 数间接符号 | **0** |
| 2 | 同上,`x86_64-apple-macos14` | **0** |
| 3 | 同上,换 LLVM 22.1.8 的 clang | **0** |
| 4 | 加上 `port/src`(11 TU)与 `openkal-macos/src`(9 TU),共 **1356 个对象** | **0** |
| 5 | `strings` 检查 lld 22.1.8 是否仍有该限制 | **仍有** `TODO: support aliasing to symbols of kind ` |
| 6 | 用 `ld64.lld`(LLVM 22.1.8)在 **Linux 上**链接全部对象 + hello | **链接成功** |

第 5 与第 1–4 条并不矛盾,它们合起来给出唯一自洽的解释:
**lld 的 N_INDR 限制仍在,但开源 clang 在这些别名上不产生 N_INDR。**
PATCHES.md 那次测量所用的编译器,与 CI 上 macOS lane 的工具链一致 ——
即 **Apple clang**。⇒ 那 15 个符号是 **Apple clang 特有的产物**,
不是这个 port 的性质,也不是 Mach-O 的性质。

#### 后果

- **`OKM_ALIAS_2` thunk 分支:撤销,不实现。** 它解决的问题在开源工具链上不存在,
  而在 Apple clang 上加它是在为一个不影响交叉的场景付代价。
- **`--ld-path=/usr/bin/ld` 仍然保留**,但它的适用范围要缩小并写清楚:
  **只在 macOS 宿主用 Apple clang 原生构建时需要**;交叉构建不经过它。
- **矩阵直接补满**(链接层面):

| 主机 ＼ 目标 | Linux | Windows | macOS | UEFI/OpenSBI |
|---|---|---|---|---|
| Linux | ✅ | ✅ | ✅ **实测** | ✅ |
| Windows | ✅ | ✅ | 🟡 同机制,未测 | ✅ |
| macOS | ✅ | ✅ | ✅ | ✅ |

⚠️ **判据的边界要说清楚:实测证明的是「链接成功」,不是「能跑」。**
产物在 Apple Silicon 上启动还需要 ad-hoc 签名,而那要在 CI 的 macOS runner 上验。
**「链接成功」与「能跑」之间隔着一次真实运行** —— 这正是本项目
「链接成功而一跑就挂」那条教训的位置。

#### ⚠️ 新发现的前置条件:mcpp 没有 Darwin target

**实测**:`mcpp toolchain list` 的目标表里有
`aarch64-linux-musl` / `x86_64-linux-gnu` / `x86_64-linux-musl` /
`x86_64-windows-gnu` / `aarch64-none-elf` / `riscv32-none-elf` / `riscv64-none-elf`,
**没有任何 `*-apple-*` 条目**。

上面的交叉链接是我手工调用 clang 与 ld64.lld 完成的;要让它成为
`mcpp build --target arm64-apple-macos14`,**mcpp 侧需要新增一行 Darwin 目标**
(ISA flags、链接线、`.tbd` 的位置、签名后处理)。这是原方案没有列出的工作项,
现补入 §11。

### 7.2 目标侧输入收进 openkal 体系

openkal-linux 已经走通了「一个都不借」:自己声明 syscall 号,自己声明内核
struct 布局(理由源码写着:*两个 C 库的 `struct stat` 不同*)。同一手法推广:

| | 现在 | 收进体系后 |
|---|---|---|
| Linux | **零** | — 已完成 |
| Windows | mingw 的 `<windows.h>` + 4 个 import lib | 自写 `.def` → `llvm-dlltool` 生成 import lib;自己声明用到的原型 |
| macOS | `-lSystem` + 1~2 符号 + `<pthread.h>` | 自写 `.tbd`(文本格式)+ 两行 `extern "C"` |

**Windows 比看上去容易**:import lib 不是微软的代码,**它只是一张
「符号 → DLL」的映射表**,mingw 自己的那些也是从手写 `.def` 生成的。
**实测(grep `openkal-windows/src`)**:实际调用的 Win32 函数约 **45 个**
(`NtCreateFile`、`WaitOnAddress`、`CreateProcessW`…)。45 个原型 + 常量,
自己声明,和 openkal-linux 自己声明内核布局是同一件事。

#### ⭐ 两条链路都已实测通过(2026-08-22,本机 Linux)

**macOS —— 恰好 2 个符号,自写 4 行 `.tbd` 即可替代 Apple SDK:**

把 1356 个对象喂给 `ld64.lld` 而不给 `-undefined dynamic_lookup`,
枚举它实际缺什么:

```
_clock_gettime_nsec_np              ← openkal-macos/src/time.cpp
_pthread_create_from_mach_thread    ← openkal-macos/src/task.cpp
__DYNAMIC                           ← 假阳性:来自 dl_iterate_phdr.c,
                                       macOS 构建本就在 mcpp.toml 里排除了它
```

**⇒ 真实缺口正好 2 个**,与 `openkal-musl/mcpp.toml` 里那句
*「two names this system's openkal implementation needs live there and nowhere else」*
逐字吻合。(⚠️ 我先前凭印象猜的是 `pthread_join`,猜错了 ——
真正的第二个是 `clock_gettime_nsec_np`。**结论会被复查,理由不会。**)

手写的 stub 全文如下,**这就是替代 Apple SDK 的全部内容**:

```yaml
--- !tapi-tbd
tbd-version:     4
targets:         [ arm64-macos, x86_64-macos ]
install-name:    '/usr/lib/libSystem.B.dylib'
current-version: 1351.0.0
exports:
  - targets:     [ arm64-macos, x86_64-macos ]
    symbols:     [ _clock_gettime_nsec_np, _pthread_create_from_mach_thread ]
...
```

产物:**691 KB 的 `Mach-O 64-bit arm64 executable`**,
`llvm-objdump --macho --dylibs-used` 显示它只依赖
`/usr/lib/libSystem.B.dylib`。全程没有任何 Apple 提供的文件。

**Windows —— 自写 `.def` → `llvm-dlltool` → `lld-link` 全链路通:**

```
llvm-dlltool -m i386:x86-64 -d kernel32.def -l libkernel32.a   → 2084 字节的 import lib
clang --target=x86_64-pc-windows-msvc -c t.c                    → COFF(自写原型,无 <windows.h>)
lld-link /nodefaultlib t.obj libkernel32.a                      → PE32+ console executable
llvm-objdump -p t.exe                                           → DLL Name: kernel32.dll
                                                                   ExitProcess / GetStdHandle
```

**⇒ §7.2 的两条路径在机制上都已验证。** 剩下的是规模问题(把 45 个原型
和完整的 `.def` 写出来),不是可行性问题。

⚠️ Windows 有一条不能照 Linux 办:**Windows 没有稳定的 syscall ABI**,
`ntdll` 就是最低的稳定层,绕不过去。所以 Windows 永远是「零第三方文件」
而不是「零外部符号」。

**价值不在省几个文件,在形态变了**:目标侧输入从「用户自备的第三方 SDK」
变成「openkal 仓库里的源码」—— 可分发、可版本化、可审计、可进索引。
**同时绕开 Apple SDK 的 EULA 灰区。**

---

## 8. 嵌入式

### 8.1 现状与两条阻塞理由

**读 `mcpp/docs/13-baremetal.md`**:

| | 状态 |
|---|---|
| C++23 语言特性 | ✅ 已可用 |
| freestanding 子集 | ✅ `import mcpplibs.std.freestanding;` |
| `import std` | ❌ *「Unavailable, and rejected at configure time」* |
| 异常 + RTTI | ❌ 整个依赖图强制关闭 |

而文档给出的**两条阻塞理由**,恰好是本方案要提供的两样:

> ① *「there is no subset of it to build **without an operating system**」*
> ② *「a freestanding target has **no compiled `libc++`** and therefore no `operator new`」*

**openkal 消 ①,openkal-llvm-runtime 消 ②。**

### 8.2 ⭐ 裸机是唯一不会给假绿的验证环境

桌面三个 OS 上有 `cxx_runtime = "host-coupled"` 这条退路 —— 可以在 C++ 运行时
**根本没上到 openkal** 的情况下让三个 OS 的 CI 全绿。裸机上没有宿主可借,
做没做完是链接器直说的:

```
ld.lld: error: undefined symbol: operator new(unsigned long)
```

⇒ **`openkal-llvm-runtime` 的验收判据是「裸机上 `import std` 能编、能链、能跑」,
不是「三个 OS CI 全绿」。**

这与本项目一路踩过的假绿教训互补:桌面环境会骗你(退路、跳过的 e2e、
从未执行的降级分支),裸机不会 —— 它没有可以静默降级的地方。

### 8.3 深嵌入式:`openkal-picolibc`(本轮纳入)

**定案:纳入范围。**

「嵌入式」是两个世界,而 openkal-musl 只覆盖其中一个:

| | 目标 | C 库 |
|---|---|---|
| **富嵌入式** | Cortex-A / RV64 SoC,MB 级 flash+RAM | `openkal-musl` |
| **深嵌入式** | Cortex-M / RV32 MCU,几十~几百 KB | **`openkal-picolibc`** |

分层允许这个替换:**openkal 是接口,openkal-musl 只是一个实现选择**。而
`std-freestanding` 已有的 `alloc-kal` / `alloc-libc` 特性轴,正是为这条路预留的形状。

⚠️ 已知的一条约束(来自 0.1 的实测):**`kal_alloc` 必须建在 libc 分配器之上** ——
picolibc 的 `vfprintf.c.o` 引用 `free`,并列会让同一块 RAM 有两个分配器。

⚠️ **体积必须实测,而工具已在**:每次 freestanding 链接都打印 `Size` 行。
`<format>` / `std::print` + 异常 + unwinder 在标准库上是几十到上百 KB 量级 ——
**进不进得了 flash 是数出来的,不是推出来的**。

### 8.4 两条保留

- ⚠️ **`import std` 在 freestanding 下是规范空白**。C++23 的 freestanding(P1642)
  没有定义 `import std` 该导出什么。合理形态是**两者并存**:能力集足够的环境上
  `import std` 可用,深嵌入式继续用 `import mcpplibs.std.freestanding`。
  按 §6.3,「能力集足够」不是一个新概念,是 libc++ 配置维度的直接后果。
- ⚠️ **异常一开,BMI 缓存立刻多一根轴**。docs/13 讲得很明白:异常开关属于
  target 而非项目 `cxxflags`,**因为 BMI 记录了它**。真正的收益是
  **从「不能开」变成「可以选」**,不是「必须开」。

---

## 9. 暂缓与不做

### 9.1 制品可移植(暂缓,记录形状)

**结论:暂缓。** 不是因为难,是因为它的输入(原生镜像格式)取决于 §5.4,
而 §5.4 取决于 II 的落地。

记录已经想清楚的部分,以免重推:

- **不发明新格式,用 ELF**。clang 直接就出;`readelf`/`objdump`/`gdb`/`perf`
  全部认识;Linux 上根本不需要转换。
- **不在运行期加载,在安装期物化**。运行期加载要面对 W^X、`MAP_JIT` entitlement、
  杀软对匿名 RX 的判定;安装期物化写的是磁盘文件,产物是**普通原生可执行文件**
  —— 双击能跑、调试器认识、不欠内核安全模型任何东西。
- **一份代码段的四个前提**:调用约定钉 psABI、自带 unwinder、TLS 统一、同一 arch。
  ⭐ 这四条**逐条对上了 APE 规范实测出来的约束**(*「APE binaries use the System V
  ABI」*、aarch64 要 `-ffixed-x28`、x86_64 TLS 要改写成调 `__get_tls_*`)——
  两条独立推导撞到同一张表,说明这张表是对的。
- **业界已有原生跨 OS 二进制**:Cosmopolitan / APE,一个文件同时是合法的
  sh 脚本、MZ 头与 x86 指令,ELF 头以八进制转义藏在前 8192 字节。它证明这条路
  走得通,也说明重造的正当理由只有一个:**openkal 的分层是规范化、可被第二个人
  独立实现的**。

### 9.2 虚拟化闭环构建期(本轮不做,备注)

**定案:本轮不做。** 备注其形状与理由,以免以后重推:

在 Windows 上用 WSL2、macOS 上用轻量 Linux VM(lima / `Virtualization.framework`)
提供统一的 Linux 用户态,可以把**构建期**闭环,收益立刻兑现(「在哪构建」这个
问题消失)。它便宜、今天就能做、且不阻塞任何东西。

**不做的理由有二:**

1. **它覆盖不了三块长期价值最高的东西**:交付给终端用户的程序(不能要求装 VM)、
   嵌入式(完全不适用)、跨 arch(Apple Silicon 上跑 x86 Linux 要模拟)。
2. ⚠️ **策略风险**:虚拟化一旦好用,会削弱做 openkal 的动力 —— 因为日常开发的
   痛点全被它解决了,而只有 openkal 能覆盖的那三块是「以后的事」。很多项目
   就是这么停在这一层的。

**它的真实成本**也应当记下:WSL2 的 `/mnt/c` 跨文件系统慢(而构建是 I/O 密集的)、
企业环境常禁 Hyper-V、常驻内存与启动延迟是**每次都付**的(openkal 的成本是
一次性的)。

---

## 10. 验证策略

### 10.1 四个承重实验

整套方案里,只有这四个问题的答案能推翻某条线。**它们加起来约一周,
而且应当先于任何工程。**

| # | 实验 | 答「否」则 | 状态 |
|---|---|---|---|
| **A** | `-femulated-tls` 能否干净套上 musl(errno / pthread / locale 全量重编 + 跑 openkal-musl 现有 32 项断言) | §9.1 死;§5.6 的 TLS 改走改写汇编路线 | 待做 |
| **B** | libc++ 能否为 openkal-musl configure 出来(先在 Linux 上,不碰交叉) | **§6 死,§8 死**,C++ 交叉打折 | 待做,**优先级 1** |
| **C** | macOS 到底借了哪些符号 | §7.2 工作量变大 | ✅ **已完成:恰好 2 个**(§7.2) |
| **D** | 不链 libSystem 的静态 Mach-O 能否被加载 | §7.2 的 macOS 部分打折 | 🟡 **已降级**:实测表明只需 2 符号的 `.tbd`,不必追求「完全不链 libSystem」。仍需 CI 验证产物能否启动 |
| **E** | ⭐ **新增**:交叉产出的 Mach-O 在真机上能否启动(含 ad-hoc 签名) | §7.1 的结论从「链接成功」升不到「能用」 | 待做,走 CI macOS runner |

**实验 C 的方法值得复用**:不要读源码猜,**把全部对象喂给链接器、不给
`-undefined dynamic_lookup`、让它列出未定义符号**。这是「枚举产出实际引用了什么」
的最省力形态,而且它顺带发现了一个假阳性(`__DYNAMIC` 来自一个在真实构建里
被排除的源文件)——**读源码不会发现这个假阳性,链接器会。**

**实验 D 的执行方式(定案)**:用 **CI 的不同 OS 资源**测,不依赖本地真机。
GitHub Actions 的 `macos-14` / `macos-15` runner 是 Apple Silicon,足以回答
这个问题。同一原则适用于其余三条 —— **凡是需要非本机环境的判据,一律走 CI**,
本机只跑 Linux 那一半。

⚠️ **实验 A 的链条比原先估计长两层**:`-femulated-tls` → compiler-rt 的
`__emutls_get_address` → pthread key → musl 的 pthread → openkal 的 task。
四层,任何一层不成立整条就断。

⚠️ 实验设计的通则(来自 0.4 的教训):**判据程序不能有无界失败模式,
尤其不能由它要找的缺陷来触发。** 先在 Linux 上跑通同一份代码,再上目标环境,
否则「被环境拒了」和「代码写错了」分不开。

### 10.2 反假绿判据

| 要验证的 | ❌ 不能用的判据 | ✅ 必须用的判据 |
|---|---|---|
| 15 个 indirect symbol 修好了 | 「链接过了」 | `nm -m` 数不到 indirect symbol **且** Linux 上 `ld64.lld` 链成 |
| unwind 注册对了 | 「编过了」 | 跨多层栈帧**真抛一次并接住** |
| C++ 运行时上到 openkal 了 | 三个 OS CI 全绿 | **裸机上 `import std` 能编能链能跑** |
| macOS 只借 1~2 个符号 | 读源码 | **枚举产出对象实际引用了什么** |
| builtins 带全了 | 链接成功 | 跑到 128 位移位那类路径 |
| picolibc 装得下 | 「链接过了」 | **`Size` 行的数字**对着目标 flash 容量 |

⚠️ 通则:**断言要放到矩阵的「行」一级**;**两侧都要断言 —— 退出码 + 内容**
(`build_fails; grep -q X || echo 丢弃` 这种写法在构建失败时没有输出,
`grep` 不匹配,被读成成功 —— 这个错误已经犯过两次)。

### 10.3 CI

- 每个仓库的 `feat/openkal-closure` 分支独立 CI
- **凡需非本机环境的判据一律走 CI**(见 §10.1 实验 D)
- ⚠️ **版本 pin 按真实需要**:上一次把 openkal 的 CI 钉在一个从裸机包抄来的
  mcpp 版本上,而 openkal 根本不需要,结果 CI 因一条不相干的发布链而红
- ⚠️ **`# requires:` 的 token 必须是活的** —— 死 token 会让整组测试从未跑过而报绿

---

## 11. 分阶段执行计划

```
第 0 步  ⭐ 规范补齐:core 定型 + 三态查询接口 + 两条原语(§5.1–5.4)
         零成本、不可逆、上游。先于一切工程。
            ↓
第 1 周  四个实验,先做 B(libc++ 能否 configure);需非本机环境的走 CI
            ↓
    ┌───────┴────────┐
并行│                │
    │ ④ §7.1 十五个  │ ⑤ §7.2 目标侧输入
    │   符号(天级)  │   (周级)
    └───────┬────────┘
            ↓
主线    ⑥ openkal-llvm-runtime(月级)+ §6.3 libunwind 静态注册
        判据 = 裸机上 import std 能跑
            ↓
        ⑦ openkal-picolibc(§8.3)—— 复用 ⑥ 的 libc++ 配置维度
            ↓
独立    ⑧ §5.6 TLS 依赖倒置(独立价值成立,不必等 §9.1)
            ↓
重估    ⑨ §9.1 制品可移植
```

**第 0 步的三条性质**决定了它必须在最前:零成本(是一个决定不是一个实现)、
不可逆(生态里一旦同时存在按不同假设写的实现,兼容性问题就开始了)、
**没有实验能回答它**。

---

## 12. 风险登记

| # | 风险 | 严重度 | 能否消除 |
|---|---|---|---|
| R1 | **设计风险**:core 边界、三态查询、命名束未落地 | **最高** | ✅ 第 0 步 |
| R2 | **验证风险**:四个承重未知全未测 | 高 | ✅ 一周实验 |
| R3 | ⚠️ **成熟度风险**:规范仍在高速发现自己的沉默 | 中~高 | 🟡 只能靠写更多完整实现 |
| R4 | ⚠️ **双重身份风险**:原生内核会变成事实参考实现 | 中 | 🟡 见下 |
| R5 | **上游同步风险**:vendored 的四个子树与上游漂移 | 中 | ✅ 第一次就定同步策略 |
| R6 | 技术风险:纯工程量 | **最低** | ✅ 已知 |

**R3 的实况**:openkal 0.5.2,实现 0.1.0~0.5.1,而 0.4 那次**写一个跨八接口的
程序,一小时挖出两条规范沉默**。规范还在这个速率上发现自己的洞,而本方案是
往上面堆两层。这不是不做的理由 —— **写一份完整的 C++ 运行时会是下一个同等强度
的探针**,它挖出来的东西比继续读规范多。但要有心理准备:**§6 的过程中规范会被改。**

**R4 的机理**:双重身份最经典的失败案例是 Win32 —— 规范和实现同一个作者、
同一个演进节奏,最后规范成了实现的文档,抽象性被吃掉。而本项目账本上已有这条
教训的雏形:**「两个实现意见一致等于零证据 —— 它们同一个作者、同一次阅读。」**

⇒ **缓解:原生内核该做,但不该被当成参考实现。规范的检验点要继续压在约束最紧的
适配实现上** —— Windows(没有稳定 syscall ABI、LLP64、PE 的 weak 不是定义)
与 macOS(不支持静态可执行、alias 编译器直接拒)。**那两个环境不会顺着规范走,
所以它们说的话才算数。**

⚠️ **R1 的一个具体形态**:`core` 的边界一旦定错,后果不对称 —— 定大了,裸机与
无 OS 环境被挡在门外(这正是 §2.2 要 core 最小的原因);定小了,只是让更多东西
落到可选能力,代价小得多。**取舍时向小的一侧倾斜。**

---

## 12.5 实施记录(2026-08-22 第一轮)

已落地并验证的部分,以及过程中发现的两处既有缺陷。

### 已合入分支 `feat/openkal-closure` 的 PR

| 仓库 | PR | 内容 |
|---|---|---|
| `openkal` | #6 | `openkal.exec`、§6.5 新条款、版本升 0.6.0、conformance 假绿修复、CI 守卫补分支形式 |
| `openkal-linux` | #4 | `openkal.exec` 的实现 |
| `openkal-macos` | #4 | `port/libSystem.tbd` —— 两个名字 |
| `openkal-musl` | #4 | 十五个间接符号的重测、`tools/probe-cross-macos.sh` |
| `openkal-windows` | #5 | 依赖改为分支形式 |

⚠️ `openkal-uefi` 与 `openkal-opensbi` 的分支**推不上去**(403,
`speak-agent` 对这两个仓库无写权限)。两者本轮无改动,不阻塞。

### ⭐ 发现的既有缺陷一:换 feature 集不会让 conformance 构建失效

同一个工作树里三次运行,别的什么都没改:

```
run-conformance.sh … full,optional   103 held, 0 not observed   (冷)
run-conformance.sh … full            103 held, 0 not observed   ← 错的
rm -rf target && … full               97 held, 1 not observed   ← 对的
```

**第二次报的是第一次那次构建。** feature 集以 define 的形式到达 suite 自己的
翻译单元,而那些 define 正是决定哪些节会检查东西的开关 —— 换了集合的那次
检查的是上一个集合,输出和退出码都不说。

⚠️ **这让 CI 的 composability 检查一直是假绿**:它在同一个检出里跑第二次
suite、用更小的集合,而它存在的全部理由就是观察「小集合被当成小集合检查」。
修复后 `core,fs,task` 报 **50 held / 7 not observed** —— 那正是它此前拿不到
的观察。

修法:记住 target 是为哪个集合建的,答案变了就丢弃;同一集合重跑仍然增量。

**这条的教训值得单独记**:一个 A/B 判据,如果 A 和 B 共用同一个构建目录,
那它测的可能是「先跑的那个」而不是「参数说的那个」。本项目账本上同型的条目
已经有三条。

### ⭐ 发现的既有缺陷二:`full` 要求每个可选接口,与 §6.1 冲突

把 `optional` 放进 `full` 后,不提供 `openkal.exec` 的实现会**链接失败**,
而 §6.1 明说「一个实现整个提供或整个不提供某接口,缺一个不是偏差」。
一个叫 `full` 却要求每个可选接口都在的集合,会让「可选」不成立,
而且报出来的是链接失败而不是一条没有成立的观察。

修法:`optional` 退出 `full`;CI 矩阵**逐行声明该实现提供哪些可选接口** ——
这与实现自己做的那个选择是同一个选择。

### 发现的既有缺陷三:CI 的「在同一版」守卫认不出分支依赖

守卫用正则从实现的 `mcpp.toml` 取 `openkal = "x.y.z"`。改成
`openkal = { git = …, branch = … }` 之后取到空串,拿空串去比对版本,
**报出来的既不是原因也不是症状**。

修法:补上分支形式 —— 那时该问的不是版本,是**分支是不是同一条**。

## 13. 已定案的八项(2026-08-22 review)

| # | 问题 | 定案 | 落在 |
|---|---|---|---|
| 1 | 规范的设计中心 | **openkal 不依赖任何 backend,是通用 ABI/SPEC 层;按自身完整性设计**。「宿主交集 vs 原生需要」这个二选一前提错误 | §2.0 |
| 2 | LLVM vendoring 范围与命名 | **只 vendored 四个 runtimes 子树;仓库名 `openkal-llvm-runtime`** | §6.2 |
| 3 | profile / core 边界 | **core 保持最小(为裸机与无 OS);其余统一为可选/配置能力;命名束是生态表达单位而非合规等级** | §2.2 |
| 4 | 能力三态的接口形态 | **加一个查询接口**,不挤进 props 位 | §5.2 |
| 5 | openarch 接不接得住 | 调研完成。**指针槽进 openarch**(与 `arch_cpu_percpu` 平行,但寄存器不同不能复用);**调用约定留在 openkal**(自我描述,先前推给 openarch 是错的) | §5.5 |
| 6 | 虚拟化闭环 | **本轮不做**,形状与理由已备注 | §9.2 |
| 7 | 深嵌入式 | **纳入**,形态为 `openkal-picolibc` | §8.3 |
| 8 | 分支名与实验 D | 分支名 `feat/openkal-closure` 确认;**实验一律走 CI 的不同 OS 资源**,不依赖本地真机 | §4.2 / §10.1 |

**尚未定的(不阻塞第 0 步,但迟早要定):**

- 命名束的**具体名字与内容边界**(§2.2 只定了原则:core 最小、其余可选)
- 三态查询接口的**签名**(§5.2 只定了「加查询接口」)
- 每上下文指针槽在 openarch 里的**具体形态**:给 `arch_context_init` 加参数,
  还是一条平行的 `arch_context_set_tls`(§5.5)
- vendored 子树的**上游同步策略**(R5)
