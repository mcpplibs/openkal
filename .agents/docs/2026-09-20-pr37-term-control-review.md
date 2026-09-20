# PR #37（KAL_TERM_CONTROL 设计稿）深度 review

- 日期：2026-09-20
- 对象：mcpplibs/openkal#37 —— `.agents/docs/2026-09-18-kal-term-control-design.md`（158 行，纯设计，无实现）
- 参照：SPEC.md（main @ 86eb855）、include/openkal/{terminal,version,stream}.h、conformance/src/sections/terminal.cpp、openkal-linux/src/terminal.cpp、openkal-macos/src/terminal.cpp、openkal-windows/src/terminal.cpp、mcpp-index/pkgs/o/openkal.lua、消费者报告 mcpplibs/openkal-musl#36
- 审查问题：**必要吗 / 通用吗 / 简洁优雅吗 / 合乎 openkal 的设计原则吗**

---

## 0. 结论

| 维度 | 判断 |
| --- | --- |
| **必要性** | **成立。** 问题真实、无法在接口之上组合出来，且正好命中条款 7.11 的准入判据 |
| **位置与形状** | **正确。** 一个模式位、落在既有 `openkal.terminal`、正交于 LINE_EDIT，是这个问题唯一自然的形状 |
| **通用性** | **基本成立，但被"ISIG 一一对应"的叙述缩窄了**（见 P3），三平台会在 `^S`/`^V`/阻塞语义上分叉 |
| **简洁优雅** | **未达到。** 方案为了一个位，附带了 (a) 修改核心条款 6.2 的规范性例外、(b) 一个强制 props 位。二者都是**极性选错**的连带损害，不是问题本身要求的 |
| **原则符合** | 7.1 / 6.4 / 7.11 符合；6.2 **不是"基本符合"，而是被本方案的极性逼成了例外**；且方案未用规范自己已有的两个发现机制（get/set 往返、`kal_version`） |

**一句话建议：保留这个位，砍掉围绕它的两处规范手术。** 把位的极性反过来（"环境放行控制键"而不是"环境消费控制键"），条款 6.2 的"未分配读 0"就自动是**真话**，强制 props 位和规范性例外一起消失，方案回到纯增量演化（条款 8）。

---

## 1. 必要性：通过

三条都查过了，不是转述设计稿：

1. **不能在接口之上组合。** openkal 无信号接口，openkal-musl 的 `rt_sigaction` 对非 SIG_DFL/SIG_IGN 一律 ENOSYS（issue #36 实测 `errno=38`）。程序既不能装 handler，也不能靠清 ICANON 顺带躲开 `^C` —— openkal-linux 的 `set_mode` 按设计只动 `ICANON`/`ECHO` 并保留其余 lflag（src/terminal.cpp:63-66 的 read-modify-write，注释明确说明为什么保留）。**"清了行编辑仍被 ^C 杀死"是当前实现的正确行为**，不是 bug，这恰好证明缺的是规范词汇。
2. **准入判据（7.11 / 历史条目 10）成立。** "三个普通程序写不出来"：全屏编辑器、分页器、自带行编辑的 REPL，都是"C 库之上预期托管的程序"。三个目标环境都有现成承载机制（termios `ISIG`、console `ENABLE_PROCESSED_INPUT`），**缺的是一个词而不是一种能力** —— 与历史条目 10（文件锁）同形。
3. **正交性论证成立。** readline 类程序"关 ICANON、保留 ISIG"是三平台的本机写法，把中断语义折进 `LINE_EDIT=0` 会让这类程序的意图不可表达 —— 与 terminal.h 里 `LINE_EDIT` 不含 `ECHO` 的既有理由（"密码提示符关一个留一个"）字面同构。设计稿 §1.1 的这一格是全文最强的一段。

> 设计稿把 issue #36 拆成"3/4 端口缺陷 + 1/4 规范缺口"是准确的分账。本 PR 只处理那 1/4，`§4 非目标`的划线干净。

---

## 2. 主要问题

### P1（首要）极性选反了，规范性例外是自找的

设计稿 §2.2 自己承认："恰好对这个位，条款 6.2 的'未分配读 0'与已发布实现的真实行为不符"。这不是这个能力的固有性质，**是 `CONTROL=1 表示环境消费控制键` 这个极性选择的后果**：

- 旧实现读回 0 → 按规范语义是"环境不消费控制键" → **假话**（它保留了 ISIG）。
- 新程序 `set(CONTROL=0)` → 旧实现忽略 → 程序以为拿到了保证 → 正是 6.2 存在要防的事故形状。

把极性反过来（暂名 `KAL_TERM_PASS_CONTROL`：**1 = 环境把约定控制键原样交给程序**）：

| | 旧实现（0.12/0.13） | 新实现 |
| --- | --- | --- |
| `get_mode` 该位 | 读 0 = "不放行" | 真实状态 |
| 真实行为 | 不放行（保留 ISIG） | 一致 |
| **6.2 的"未分配读 0"** | **是真话** | 是真话 |
| 程序 `set(位)` 后读回 | 读回 0 → **程序当场知道没拿到** | 读回 1 |

于是：

- 条款 6.2 **一个字都不用改**，本方案变回纯粹的条款 8 演化；
- **强制 props 位不再必要**（能力发现由 get/set 往返承担，见 P2）；
- 发现机制用的是规范自己已经写下并且 conformance 已经在观察的那一条 —— terminal.h："A position this implementation does not distinguish is reported as zero by the first and ignored by the second"，conformance/src/sections/terminal.cpp 已有 "the mode read back is the mode that was set"；
- 也符合 7.11 "问询的逆"：程序通过逆运算得知环境的实际状态，而不是另开一条"先查 props"的特殊规矩。

**唯一代价**是位的语态与 `LINE_EDIT`/`ECHO`（"环境做 X"）不完全同向。但 `PASS_CONTROL` 仍是描述环境行为的（"环境放行"而非"程序要求"），语态一致；而用语态上的整齐去换一条核心条款的例外 + 一条程序必须记住的新规矩，**这笔买卖是亏的**。openkal 的风格恰恰是宁可位名拗口也不让规范长出例外（见历史条目 8 对"一句 openkal 不会有 fork"的拒绝理由）。

### P2 强制 props 位改变了 props 字的粒度，且没有停止点

现行 `kal_terminal_props` 的两个位是**操作级**的：`KAL_TERM_PROP_MODE`（"get/set 被回答"）、`KAL_TERM_PROP_SIZE`（"显示尺寸可知"）。`KAL_TERM_PROP_CONTROL` 是第一个**位级**条目 —— 它回答的是"模式字里某一位是否被区分"。引入之后：

- 为什么 `LINE_EDIT`/`ECHO` 没有对应的 props 位？三个模式位里两个靠 get/set 往返发现、一个靠 props 发现，读者无法从字面推出规则；
- 按同一逻辑，**今后每加一个模式位都要加一个 props 位**，props 字变成模式字的影子，且没有原则能在中途叫停。这与条款 6.2 划的"操作 / 属性"两分不是一个东西。

若采纳 P1，这个位可以整个不要；若要保留（作为可选的、便于一次性询问的冗余信息），也**不应写成"程序须先查 props"的规范义务**，否则等于承认模式字的 get/set 往返不可信 —— 而那是 terminal.h 与 conformance 已经承诺的东西。

### P3 "ISIG 一一对应"不足以兑现 §2.1 的保证，三平台会分叉

设计稿 §2.1 的承诺是"**保证每个键以字节到达**"。termios 上 `ISIG` 只挡住 `VINTR/VQUIT/VSUSP`，其余仍被线路规程吃掉：

| 被环境吃掉的键 | termios 开关 | 设计稿是否覆盖 | Windows `ENABLE_PROCESSED_INPUT` |
| --- | --- | --- | --- |
| `^C` `^\` `^Z` | `ISIG` | 是 | 是 |
| `^S` `^Q`（流控） | `IXON` | **否** | 是（同一开关） |
| `^V` `^O`（literal-next / discard） | `IEXTEN` | **否** | n/a |
| `Enter` 变成 `0x0a` | `ICRNL` | 否（可另议） | n/a |

后果：同一个模式字下，Linux/macOS 的 `^S` 仍会冻住终端而 Windows 不会 —— **由规范导致的跨平台行为分叉**，正是 7.1 要避免的"规范取了某一个环境的形状"。musl `cfmakeraw` 清的是 `ISIG|IEXTEN|ECHO|ECHONL|ICANON` 加 `IXON` 等，所以 §3 "现有全屏程序不改动即正确"也随之**过度宣称**。

修法很轻，且不损 7.1：把位定义成 **"环境是否保留*一组*约定控制键为己用，集合由环境定义"**，实现侧把它映射到本环境**全部**此类机制（termios: `ISIG|IXON|IEXTEN`；console: `ENABLE_PROCESSED_INPUT`）。方向性保证写成非对称的两句 —— **0 方向是承诺（所有按键都是数据），1 方向是许可（环境可保留，保留哪些由环境定）** —— 设计稿 §2.1 已经隐约这么写了（"可保留约定控制键"），只是 §1.2 的表和任务卡 3a/3c 又把它收窄回了 `ISIG` 一个标志。把任务卡也改成全集即可。

**附带的硬问题：`VMIN`/`VTIME`。** `stream.h` 写死"Zero denotes end of input"。而 `ICANON` 清掉之后，读的阻塞语义由 `c_cc[VMIN]/[VTIME]` 决定，而模式字里没有位承载它，现行三个实现也都不碰它（openkal-linux src/terminal.cpp 的 read-modify-write 只动 lflag）。若终端被前一个程序留在 `VMIN=0`，`kal_stream_read` 会返回 0，**程序读到一个不是 EOF 的 EOF**。这是本设计使全屏程序真正可写之后立刻会撞上的下一格。建议至少二选一：任务卡 3a/3b/3c 要求"清 LINE_EDIT 时确保至少一字节才返回（`VMIN=1, VTIME=0`）"；或按 §11 的体例记一条"未settle"条目。**不宜默默留白**——留白正是 issue #36 那份报告全程在批评的形状。

### P4 规范已有的发现机制之一是坏的：`KAL_VERSION_MINOR` 停在 11

`include/openkal/version.h:36` 是 `11u`，而 mcpp.toml 是 `0.13.0`。而 version.h 开头那段话，字面就是 §2.2 想解决的问题：

> "A consumer compares it with what `kal_version' answers and refuses to proceed against an implementation older than the declarations it holds --- because an older implementation reports conditions this consumer distinguishes as conditions it does not, **which is a wrong answer rather than a refusal**."

"旧实现把它不区分的状态报成 0，是错误答案而不是拒绝" —— 规范早就写下了这条规则和它的对策。设计稿 §5.2 却把版本停滞当成"仓库惯例"接受下来（"0.12/0.13 发布均未 bump version.h"）。**那不是惯例，是漂移**：对 linked consumer 无所谓（靠 get/set 往返与链接期符号），但对条款 3.2 点名的那类消费者 —— 在 load 期绑定或跨边界、没有链接器可问的 —— `kal_version` 是唯一入口，而它现在会说 0.11。

建议：0.14 这一波把 version.h 校正到 `0u.14u.0u`，并在 PR 里写明为什么（漂移、以及本设计正好是第一个依赖它的位）。这比新增一条 6.2 例外更贴合规范自身的结构。

### P5 三处引用与事实核对不符（都在"事实来源"里，需改）

1. **§2.1 "SPEC.md:284-304 明确排除[信号]"** —— 该行区间是条款 4.2 的"Arrangements considered and not adopted"，讲的是声明的组织方式，与信号无关。**全文 grep：SPEC.md 中没有任何排除信号接口的条款**（"signal" 只出现在 §11 历史条目 9 讨论 job control 的语境里）。"openkal 无信号"目前只见于 README.md:115 和 terminal.h 的一句注释（"openkal has no signals, so a program learns of a change by asking again"）。
   → 本设计的整条论证都架在"无信号"之上（"这是全屏程序在 openkal 上存活于 ^C 的唯一途径"）。**既然 0.14 因为"没有信号"才必须加这个位，就应当在同一个 PR 里把这项排除写进 §3.4 或 §11** —— 正是历史条目 8 的体例（"一句'openkal 不会有 fork'会把这个区分埋掉，此条因此存在"）。这一条我认为不是可选润色，而是本设计缺的那块规范文字。
2. **"SPEC 历史条目 7（文件锁）"** —— 条目 7 是 links；文件锁与那句 "What was missing was a word, not a capability" 在**条目 10**（SPEC.md:1105-1115）。论点没错，编号错了。
3. **§5.1 "仿 register-openkal-0.15.0 分支既有流程"** —— mcpp-index 当前该分支名与 openkal 实际版本（index 里最高 0.13.0）不一致，容易被读成"0.15 已在路上"。引用流程即可，别引用这个分支名。

### P6 位名 `KAL_TERM_CONTROL` 语义含糊

`CONTROL` 单独一个词既可读成"控制键"，也可读成"对终端的控制"，而同一个头文件里 `LINE_EDIT`/`ECHO` 都是**动作**。设计稿 §2.3 排除 `INTERRUPT`/`SIGNAL` 的理由（7.5 已占用 "Interruption"、openkal 无信号模型）是对的，但结论落点不够。建议 `KAL_TERM_PASS_CONTROL`（配合 P1 的反相极性）或 `KAL_TERM_CONTROL_KEYS`（维持原极性）。

---

## 3. 次要

- **§1.1 表里 "CONTROL 1（默认）"** —— 模式字报告的是"当前生效的模式"，没有"默认"一说；准确的话是"三个环境的终端初始状态中该位通常为 1"。
- **R2（conformance 可观测性）的折衷是对的**，但可以再收一格：CI 无 pty 时观察不了"^C 以字节到达"，那就把**非交互流的拒绝**与**位的 get/set 往返**当作可观测的两半（现有 section 已是这个骨架），端到端留给 issue #36 的探针。建议把探针程序收进 openkal-musl 仓库而不是只留在 issue 里 —— 链接会烂，验收会跑不了。
- **任务卡 4 的验收**（"两支探针在 pty 下与 glibc 对照一致"）目前是人工步骤，且第二支探针测的是 SIG_IGN —— 而 SIG_IGN 已被 §4 划出范围。两处需对齐：要么验收只用第一支探针，要么把 SIG_IGN 修复并入同批（即 O1，我倾向并入：issue #36 的四个缺陷里三个是"报告成功却什么都没做"，那是该端口文件自己的注释声明绝不产生的形状，分批修会让消费者拿到一个"raw mode 好了但 SIG_IGN 仍骗人"的中间态）。
- **依赖序**（openkal → index → 三实现并行 → musl）**正确**，理由也站得住：三实现只共同依赖规范，musl 依赖实现跑端口级验证。无可改。
- **零改动宣称**：请按 P3 降级为"`cfmakeraw` 中与模式字对应的部分自动正确"，别写成"现有全屏程序不改动即正确"。

---

## 4. 如果采纳，0.14 的最小改动清单

1. `terminal.h`：加一个模式位，**反相极性**，含房屋风格的理由注释（为什么是第三个位而不是折进 LINE_EDIT；为什么极性与前两位相反 —— 因为"未分配读 0"必须是真话）。`src/terminal.cppm`、`src/macros.cppm` 镜像。
2. `SPEC.md`：**不动条款 6.2**；加一条 §11 条目记录本位的admission（体例照条目 10），并在 §3.4 或 §11 补记"信号接口不被定义"这项既有立场（P5-1）。
3. `include/openkal/version.h`：`KAL_VERSION_MINOR` 校正为 14（P4），PR 里说明漂移。
4. 不加 props 位；若坚持加，则写成可选的冗余信息，不写"程序须先查"的义务（P2）。
5. conformance：往返观察沿用现有骨架，新增位纳入同一条观察。
6. 三实现任务卡：映射**全集**（`ISIG|IXON|IEXTEN` / `ENABLE_PROCESSED_INPUT`），并处理 `VMIN/VTIME`（P3）。

## 5. 如果坚持原极性，必须补的文字

- 条款 6.2 的例外必须写成**一般规则**而不是"恰好对这个位"：即"当某位的 0 值同时是一个**真实状态**而非能力缺席时，该位须伴随 props 位，且程序须先查 props"。否则这条例外无法泛化，下一个同形的位会重开一次同样的讨论——而 openkal 的体例一贯是把规则写在能泛化的高度上（条款 7.11 之于 `kal_fs_set_modified` 就是范例）。
- 并且仍需 P3、P4、P5 的修正 —— 这三条与极性选择无关。
