# openkal.terminal 的第三个模式位：设计与跨仓库实施计划（v2）

- 日期：2026-09-20
- 状态：设计稿，待 review（不含实现；实现按 §6 任务图在各仓库另行提 PR）
- 关系：取代 `.agents/docs/2026-09-18-kal-term-control-design.md`（PR #37）的方案部分，保留其问题分析与依赖序；差异集中在 §2（位的极性）、§3（不动条款 6.2、不加 props 位）、§4（保证的范围与读的零）、§5（发现机制修复）
- 起因：消费者报告 [mcpplibs/openkal-musl#36](https://github.com/mcpplibs/openkal-musl/issues/36)——程序无法让"每个按键以字节到达"，因而被 `^C` 杀死
- 涉及仓库：`openkal`（规范）、`mcpp-index`（发布）、`openkal-linux`/`-macos`/`-windows`/`-emscripten`（**四个**实现，均宣告 `KAL_IFACE_TERMINAL`）、`openkal-musl`（C 环境之一，消费者侧适配）
- 事实来源（均已本地核对，锚定 main @ `86eb855`）：
  - SPEC.md 条款 6.1/6.2/6.4/7.1/7.4/7.11、§3.2、§11 历史条目 10（文件锁）与 8（fork）
  - `include/openkal/terminal.h`（模式字两位、get/set 是逆）、`include/openkal/version.h:30-36`、`include/openkal/stream.h:26-28`
  - `conformance/src/sections/terminal.cpp`（已有往返观察）、`tools/gen-macros.sh`、`tools/check-readme-versions.sh`、`SURFACE.txt`
  - 四个实现的 `src/terminal.cpp`；`openkal-linux/src/version.cpp:16`（`kal_version` 返回 `KAL_VERSION`）

---

## 0. 一句话

**模式字加一个位，极性取"放行"而不是"消费"——`KAL_TERM_PASS_CONTROL`：1 = 环境不为自己保留任何按键，每个按键以字节到达；0 = 环境可保留一组由自己定义的按键。** 极性这样取，条款 6.2 的"未分配读 0"对旧实现**是真话**，于是不需要规范例外、不需要强制 props 位、不产生语义债；发现机制用规范已有的 get/set 往返。附带两项与位同源的收尾：把"读到 0 即输入结束"（7.4）在关掉行组装时的后果写明，以及修好整个生态里已经失灵的版本发现机制（`KAL_VERSION_MINOR` 停在 11）。

---

## 1. 需求（与任何一种 C 环境无关）

逐键程序的需求只有一句：**我读到的每个字节，就是用户按下的每个键**。三种场景：

| 场景 | LINE_EDIT | ECHO | PASS_CONTROL | 例子 |
| --- | --- | --- | --- | --- |
| 全屏程序 | 0 | 0 | **1**（连 `^C` 也是数据） | 编辑器、分页器、游戏、TUI |
| 自带行编辑的程序 | 0 | 0 | **0**（`^C` 仍由环境处置） | shell 的行编辑、REPL |
| 普通程序 | 1 | 1 | 0（终端的常态） | 一切照旧 |

第二行是硬约束：它否决了"把中断语义折进 `LINE_EDIT=0`"的省事方案，所以这必须是**正交的第三个位**——与 terminal.h 里 `LINE_EDIT` 不含 `ECHO` 的既有理由（密码提示符"关一个留一个"）同构。

**这不是一个 C 库的需求，更不是 musl 的需求。** 需要它的是"一个程序 + 一个交互流"，与程序用什么语言写、上面盖没盖 C 库无关：`openkal-musl` 会用它兑现 `cfmakeraw`，`std-freestanding` 的 TUI 程序直接用模块形式 `kal::terminal::pass_control`，一个不带 C 库的裸程序也一样。issue #36 是**证据**，不是设计的驱动方。

### 1.1 为什么不能在接口之上组合出来（必要性）

- openkal 不定义信号接口，程序装不了 handler；
- 清掉 `LINE_EDIT` 不附带任何中断语义——四个实现的 `set_mode` 都只动自己命名的两个位并保留其余（linux `src/terminal.cpp:52-80` 的 read-modify-write 注释写明了为什么保留），这是**正确行为**，不是缺陷；
- 于是"每个按键都是数据"在当前规范下**无法表达**。缺的是一个词，不是一种能力：每个目标环境都有现成机制，拼写几乎一致——这正是 §11 历史条目 10（文件锁）记下的准入形状。
- 条款 7.11 的准入判据（"三个普通程序在接口之上写不出来"）满足：编辑器、分页器、自带行编辑的 REPL。

---

## 2. 方案：一个位，极性取"放行"

### 2.1 声明（`include/openkal/terminal.h`）

```c
/* Positions in the mode word.
 *
 * A position that has not been assigned reads as zero, so a program compiled
 * against a later revision of this specification behaves correctly against an
 * earlier implementation (clause 6.2). A POSITION IS THEREFORE GIVEN THE SENSE
 * IN WHICH ZERO IS THE WEAKER CLAIM: an implementation that has never heard of
 * a position must not be made to state the stronger one by its silence. */
#define KAL_TERM_LINE_EDIT    ((kal_uintptr)1u << 0)  /* the environment assembles lines     */
#define KAL_TERM_ECHO         ((kal_uintptr)1u << 1)  /* the environment shows what is typed */
#define KAL_TERM_PASS_CONTROL ((kal_uintptr)1u << 2)  /* the environment reserves no keystroke */
```

语义，写在位的旁边（规范性文字在声明里，SPEC.md 从不重述模式位）：

- **`PASS_CONTROL = 1` 是一个保证**：环境不为自己保留任何按键，用户按下的每个键都以它产生的字节到达程序——包括 `0x03`。
- **`PASS_CONTROL = 0` 是一个许可**：环境**可以**保留一组按键用于自身动作。**保留哪些由环境定义**，规范不列举、不要求一致。
- 方向不对称是有意的：程序需要的是 1 方向的保证；0 方向若要求各环境一致，就是要求每个环境实现别人的键表，条款 7.1 拒绝这种形状。

### 2.2 为什么极性是"放行"而不是"消费"（本设计的核心）

同一个位有两种取法，差别只在旧实现（0.12/0.13，不认识这个位）读回 0 时**说的是真话还是假话**：

| 极性 | 旧实现读 0 的含义 | Linux/macOS/Windows 的真实行为 | 旧实现说的是 |
| --- | --- | --- | --- |
| 消费（PR #37） | "环境不消费控制键" | 保留 ISIG，**会消费** | **假话**，而且是危险方向的假话：程序以为拿到了保证，然后被 `^C` 杀死 |
| **放行（本稿）** | "环境不放行控制键" | 确实不放行 | **真话** |

再看另一端，一个天然没有线路规程的环境（裸机控制台、将来的 UEFI/串口实现）：旧实现读 0 = "不放行"，而它其实一直放行——**这也是假话，但方向是保守的**：程序被告知"拿不到保证"，于是降级或拒绝运行，没有人死掉。

**这就是条款 6.2 的"未分配读 0"一直以来的含义：0 是弱主张、是能力缺席。** PR #37 之所以需要给 6.2 开例外，不是因为这个能力特殊，而是因为极性取反了之后 0 变成了强主张。极性改回来，例外自动消失：

- 条款 6.2 **一个字都不用改**，本设计回到条款 8 的纯增量演化；
- **不需要强制 props 位**（见 §3.1）；
- **没有语义债**：已发布的 0.12/0.13 在新规范下读回 0，仍然是对自己行为的正确描述。PR #37 的 R1（"旧行为应视为 bug、必须先还债才能诚实宣称"）在本方案下不存在。

### 2.3 发现机制：用规范已有的那一个

程序要问"我能不能拿到保证"，答案从 get/set 的往返里来，这是 terminal.h 已经承诺、conformance 已经在观察的东西：

```c
kal_uintptr m;
kal_terminal_get_mode(s, &m);                       /* 先拿到可恢复的原样 */
kal_terminal_set_mode(s, (m & ~KAL_TERM_LINE_EDIT & ~KAL_TERM_ECHO)
                          | KAL_TERM_PASS_CONTROL);
kal_uintptr got;
kal_terminal_get_mode(s, &got);
if (!(got & KAL_TERM_PASS_CONTROL)) { /* 这个实现或这个资源给不了，降级 */ }
```

- 旧实现：忽略未知位、读回 0 → 程序当场知道，且终端未被改坏；
- 新实现但资源给不了（串口、浏览器里的 emscripten）：同上，**按资源回答**，这正是 `kal_terminal_props` 作为"取资源的问询"而非静态字的理由（条款 6.2 末段）；
- 三行、无新概念、无新位、无调用顺序上的新规矩。

### 2.4 命名

`KAL_TERM_PASS_CONTROL`，注释 `the environment reserves no keystroke`。

- 不叫 `INTERRUPT`/`SIGNAL`：条款 7.5 已占用 "Interruption"（含义完全不同），且 openkal 不定义信号；
- 不叫单个 `CONTROL`：既可读成"控制键"，也可读成"对终端的控制"，而同一个头文件里另外两位都是明确的动作；
- 备选（供 review 定夺）：`KAL_TERM_KEYS_AS_DATA`（最直白，但语态从"环境做什么"变成"程序得到什么"）、`KAL_TERM_PASS_KEYS`（更宽，但"控制键"这个限定正是它要表达的）。

### 2.5 考虑过而未采纳

- **折进 `LINE_EDIT=0`。** 不采纳：自带行编辑的程序要"关行组装、留中断"，折叠使其意图不可表达（§1 第二行）。
- **配一个强制的 `KAL_TERM_PROP_CONTROL`。** 不采纳，见 §3.1。
- **按键逐个命名（中断键、挂起键、流控键各一位）。** 不采纳：等于要求每个环境实现别人的键表，条款 7.1 的机械判据（翻译表 / 注册表）当场判否；而程序的需求是全有或全无。
- **给 0 方向规定统一的键集合。** 不采纳，同上；0 方向定为许可而非保证。
- **用 `kal_version` 作为主要发现机制。** 不采纳为**主要**机制：版本是"实现级"的答案，而终端的能力**按资源变化**（同一个实现，pty 能、串口不能）。版本仍需修好，但用途是 §5 的那一类消费者。

---

## 3. 与条款的逐条核对（结论与理由，不含客套）

| 条款 | 结论 | 理由 |
| --- | --- | --- |
| 6.2 未分配读 0 | **符合，且不需例外** | 极性使 0 成为弱主张；见 §2.2 |
| 6.1 接口的存在 | 符合 | 不新增操作，无"存在但总是失败"的形状 |
| 6.4 不可一致存在的操作 | 符合 | 语义只存在于交互流，落在既有 `openkal.terminal`，不开新接口、不动 `openkal.stream` |
| 7.1 自然性 | 符合 | 四个实现各自 1-3 行本机映射，无翻译表、无注册层；位的措辞不提任何环境的机制名 |
| 7.4 传输 | **需补一句后果**，见 §4 | 关掉行组装后若读返回 0，就与"零即输入结束"冲突 |
| 7.11 问询的逆 | 符合，且被强化 | 发现机制就是那个逆；get 拿到的仍是可原样恢复的模式字 |
| 3.2 核心集不扩张 | 不冲突 | `openkal.terminal` 是可选接口；加位属条款 8 |
| 5.3 结构布局不可变 | 不涉及 | 不动任何结构 |
| 9 一致性程序 | 部分可观测，见 §7 R2 | `SURFACE.txt` 不变（不新增符号）；宏经 `tools/gen-macros.sh` 自动进 `src/macros.cppm` |

### 3.1 为什么不加 props 位

现行 `kal_terminal_props` 的两个位是**操作级**的：`PROP_MODE`（get/set 被回答）、`PROP_SIZE`（尺寸可知）。加一个**位级**的 `PROP_CONTROL` 会带来三件事，一件比一件贵：

1. 粒度不一致：三个模式位里两个靠往返发现、一个靠 props 发现，规则从字面推不出来；
2. 没有停止点：按同一逻辑，此后每加一个模式位都要加一个 props 位，props 字沦为模式字的影子；
3. 它只在"极性取反"的前提下才**必须**存在——而那个前提本身是本设计要去掉的东西。

`openkal-emscripten/src/terminal.cpp` 的做法是这条判断的现成佐证：它的 props 是**问机器**得来的（浏览器里没有终端就不宣告），注释写着"a program that believes it turned echo off and did not is a program that prints a password"。既然能力本来就按资源问、按资源答，就不该把一个位的存在性再搬进静态字里。

---

## 4. 同源的第二件事：关掉行组装之后，读的零

条款 7.4 是规范性的："A result of zero denotes end of input."（`stream.h:26-28` 同文）。而行组装一关，读的阻塞语义就由环境里**另一组旋钮**决定（termios 的 `VMIN`/`VTIME` 是其中一种形式），模式字不承载它，四个实现目前也都不碰它。后果是：终端若被上一个程序留在"可以立刻返回零字节"的状态，`kal_stream_read` 返回 0，**程序读到一个不是输入结束的输入结束**——这正是本设计让全屏程序终于可写之后，它们会立刻踩到的下一格。

**不新增位，也不新增概念**，只把既有条款的后果写在它该在的地方（terminal.h，`set_mode` 旁）：

> An implementation that turns line assembly off shall not thereby cause a read
> of the stream to report zero while input has not ended: zero denotes end of
> input (clause 7.4), and a mode change does not make it mean anything else.

实现侧的代价：清行组装位时顺手把本环境的"至少等到一个字节"旋钮置好（termios 上是 `VMIN=1, VTIME=0`，一行）。

这一条可与位同批，也可拆成 0.14 的第二个提交；它**不依赖**位，但由位引出，分开走会让消费者拿到一个"raw mode 好了、read 偶尔假 EOF"的中间态。

---

## 5. 同源的第三件事：版本发现机制已经失灵

`include/openkal/version.h:36` 是 `KAL_VERSION_MINOR 11u`，包是 `0.13.0`；而每个实现的 `kal_version()` 就是 `return KAL_VERSION;`（openkal-linux/src/version.cpp:16，其余同）。于是**整个生态的实现都在报 0.11**，version.h 开头那段话所描述的比较——

> "A consumer compares it with what `kal_version' answers and refuses to proceed
> against an implementation older than the declarations it holds --- because an
> older implementation reports conditions this consumer distinguishes as
> conditions it does not, **which is a wrong answer rather than a refusal**."

——恒等于"相等"，形同虚设。对被链接的消费者无所谓（有链接器和往返可问）；对条款 3.2 点名的那一类——**在 load 期绑定、或跨越一个用陷入实现的边界**，没有链接器可问的消费者——`kal_version` 是唯一入口。本设计是第一个需要它说真话的位。

两步，都很小：

1. `version.h` 校正为 `0/14/0`，并在 PR 里写明这是漂移的修正而不是惯例的延续；
2. 加一条 CI 检查（新增 `tools/check-version.sh`，或并入 `tools/check-readme-versions.sh`）：`KAL_VERSION_*` 必须与 `mcpp.toml` 的 `version` 一致。

第 2 步是这个仓库自己的体例：`check-readme-versions.sh` 的注释说得很清楚——"THE POINT IS NOT THE STALENESS, IT IS THAT IT WAS INVISIBLE... The one thing a reader actually types was checked by nobody." 把"读者要抄的那一行"换成"消费者要问的那一个数"，同一句话成立。

---

## 6. 生态与任务图

### 6.1 实现是四条腿，不是三条

| 仓库 | 环境里的承载机制 | 改动量 |
| --- | --- | --- |
| `openkal-linux` | termios：`ISIG`、`IXON`、`IEXTEN`（**全集**，见下） | `mode_of` 与 `set_mode` 各 1-2 行 |
| `openkal-macos` | 同上（POSIX 同构） | 同上 |
| `openkal-windows` | console：`ENABLE_PROCESSED_INPUT` | 同上 |
| `openkal-emscripten` | 转发宿主 termios；浏览器里 props 本就不宣告 | 同上，且天然按资源作答 |

**映射取全集而不是只取 `ISIG`。** 保证的措辞是"每个按键以字节到达"，而 termios 上 `ISIG` 只挡住中断/退出/挂起三键；流控（`^S`/`^Q`，`IXON`）和 literal-next/discard（`^V`/`^O`，`IEXTEN`）仍会被线路规程吃掉。Windows 的 `ENABLE_PROCESSED_INPUT` 恰好**包含**流控。只映射 `ISIG` 的结果是：同一个模式字下 Linux 的 `^S` 冻住终端而 Windows 不会——由规范导致的跨平台分叉，正是条款 7.1 要避免的"规范取了某一个环境的形状"。位的措辞（"环境保留的按键，集合由环境定义"）已经把这件事说对了，任务卡只需与它一致。

将来一个没有线路规程的环境（裸机、UEFI 控制台）实现 `openkal.terminal` 时：`PASS_CONTROL` 恒为 1，`set(0)` 被忽略，读回 1——诚实，且不需要它去模拟任何人的键表。

### 6.2 消费者是多个，`openkal-musl` 只是其中之一

| 消费者 | 本设计对它意味着什么 |
| --- | --- |
| `openkal-musl` | `tcgetattr`/`tcsetattr` 的 `ISIG|IXON|IEXTEN` ↔ `PASS_CONTROL`；`cfmakeraw` 随之免费正确 |
| 任何其他 C 环境（picolibc 一类的移植） | 同一映射，各自决定；规范不提 termios，所以不与 musl 绑死 |
| `std-freestanding` / 不带 C 库的程序 | 直接用模块形式 `kal::terminal::pass_control`，不经过任何 C 语义 |
| 已有 TUI 类端口（imgui 等） | 现有全屏程序在**与模式字对应的那部分**自动正确；其余（输出后处理、字符尺寸/奇偶等）仍由各自环境决定，**不宣称"零改动即全对"** |

### 6.3 任务图与顺序

```
1. openkal 0.14.0（规范，唯一硬先行）
   ├ terminal.h：KAL_TERM_PASS_CONTROL + 位的语义 + §4 的一句 + 极性规则一句
   ├ src/terminal.cppm：kal::terminal::pass_control
   ├ tools/gen-macros.sh 重生成 src/macros.cppm（CI 会 diff）
   ├ SPEC.md：§11 新条目（体例照条目 10）；条款 6.2 加"零是弱主张"一句（规则，非例外）
   ├ version.h 0.14.0 + tools/check-version.sh（§5）
   ├ conformance：往返观察纳入新位；新增"关行组装后读不返回假零"的条件观察
   └ mcpp.toml / README 版本行（check-readme-versions.sh 会校）
        │ 合并 + tag 0.14.0
        ▼
2. mcpp-index 注册 openkal 0.14.0
        │
        ├──────┬──────────┬──────────────┐
        ▼      ▼          ▼              ▼
3a. linux  3b. macos  3c. windows  3d. emscripten      （四实现并行，互不依赖）
        └──────┴──────────┴──────────────┘
                     │ 各自 minor bump + index 注册
                     ▼
4. 消费者侧：openkal-musl（ioctl 分发器路由到 kal_terminal_*；
   并修掉三处"报告成功却什么都没做"：TCGETS/TIOCGWINSZ 不回写、SIG_IGN 不安装）
                     │
                     ▼
5. 验证与回执：issue #36 的两支探针收进 openkal-musl 的 tests/（不留在 issue 里），
   pty 下与宿主 C 库对照；回帖 #36
```

**为什么是这个序**：下游都要 `#include <openkal/terminal.h>` 的新位，且 mcpp 依赖解析要求 `openkal = "0.14.0"` 先存在于 index；四个实现只共同依赖规范、彼此独立；消费者层依赖实现跑端到端验证，且改动最大，放最后风险最低。

### 6.4 验收

| # | 验收 |
| --- | --- |
| 1 | `tools/check-*.sh` 全绿（含新增的版本检查）；CI 四声明矩阵绿；`macros.cppm` 重生成无 diff |
| 2 | index CI 绿 |
| 3a-3d | 各自端口自测 + openkal conformance；有 pty 的腿上人工确认 `^C` 以 `0x03` 到达、`^S` 不冻结 |
| 4 | 探针程序在 pty 下与宿主 C 库对照一致；三处 silent success 消失（哨兵字节被真实数据覆盖、`SigIgn` 位真的出现） |
| 5 | #36 上给出可复现的对照输出 |

---

## 7. 风险与开放问题

- **R1 无语义债**（与 PR #37 最大的差别）。旧实现读回 0 在新规范下仍是真话，因此不存在"必须先升级才能诚实"的前置；升级顺序纯由依赖决定。
- **R2 一致性套件看不见端到端保证。** CI 无 pty 且套件必须能在没有 C 库的环境跑，所以自动观察只到"往返"和"非交互流的拒绝"两半；"`^C` 以字节到达"由 §6.3 第 5 步的探针承担。探针进仓库，不留在 issue（链接会烂）。
- **R3 名称待裁。** `PASS_CONTROL` / `KEYS_AS_DATA` / `PASS_KEYS`，见 §2.4。
- **R4 §4 与 §5 是否同批。** 两者都可拆；建议同批，理由分别是"分开会留下中间态"和"本设计是第一个需要版本说真话的位"。
- **R5 条款 6.2 的那一句是否写。** 不写也不影响本设计成立（极性已经把事办了）；写了则把一次性判断变成下一个位也适用的规则——这是 openkal 一贯的落笔高度（条款 7.11 之于 `kal_fs_set_modified`，历史条目 8 之于 fork）。倾向写。
- **O1 `openkal-musl` 的三处 silent success** 与本设计无关，属端口缺陷，但建议同批：issue #36 的四个缺陷里三个是"报告成功却什么都没做"，分批修会让消费者拿到"raw mode 好了但 `SIG_IGN` 仍骗人"的中间态。
- **O2 输出方向暂不处理。** 输出后处理（termios `OPOST` 一类）不在模式字里，本设计不动它；若将来有程序因此写不出来，按同一判据（7.11）再议，并按同一极性规则取位。

---

## 8. 附：待 review 的规范性文字草稿

**条款 6.2（在"a position that has not been assigned reads as zero"之后加一句）：**

> A position is assigned the sense in which zero is the weaker claim, so that an
> implementation which has never heard of the position is not made to assert the
> stronger one by its silence.

**SPEC.md §11 新条目（编号 20，体例照条目 10）：**

> 20. **A keystroke the environment keeps for itself.** Settled in 0.14.
>     `KAL_TERM_PASS_CONTROL` states whether the environment reserves any
>     keystroke, and a program that needs every key as data asks for it and reads
>     the mode back. Admitted on the grounds entry 10 records for locking: every
>     environment this specification has been implemented on already reserves
>     such keys and spells the switch almost identically --- `ISIG` and its
>     neighbours, `ENABLE_PROCESSED_INPUT` --- so what was missing was a word and
>     not a capability.
>
>     **What its absence cost, and it was not a refusal.** A C environment above
>     this interface answers `tcsetattr` with the mode word. With no position for
>     the reserved keys, an editor that cleared line assembly and echo was still
>     killed by the interrupt key, and nothing in the interface was wrong: the
>     implementation preserved what the specification had not named. Measured
>     against a host C library on one terminal, openkal-musl#36.
>
>     **The position is spelled in the sense that zero is the weaker claim**, so
>     an implementation predating it reports zero and thereby says something true
>     about itself. The arrangement in which zero would have been the guarantee
>     was considered and not adopted: it would have required an exception to
>     clause 6.2 and a property position beside it, to repair a hazard that the
>     spelling itself removes.
