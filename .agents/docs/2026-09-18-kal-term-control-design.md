# openkal.terminal 增加 KAL_TERM_CONTROL 位：设计与跨仓库实施计划

- 日期：2026-09-18
- 状态：设计稿，待 review（本文档不含实现，实现按 §5 任务图在各仓库另行提 PR）
- 起因：消费者报告 [mcpplibs/openkal-musl#36](https://github.com/mcpplibs/openkal-musl/issues/36)——`tcsetattr` 被端口拒绝（ENOTTY），程序无法进入 raw mode，也无法阻止 Ctrl+C 杀死进程
- 涉及仓库：`mcpplibs/openkal`（规范），`mcpplibs/mcpp-index`（发布），`openkal-linux`、`openkal-windows`、`openkal-macos`（实现），`mcpplibs/openkal-musl`（C 库端口）
- 事实来源（规范文档地址，链接锚定 main 当前头 `86eb855`）：
  - 规范正文：[SPEC.md](https://github.com/mcpplibs/openkal/blob/86eb855/SPEC.md)——条款 6.2（未分配位读 0）、6.4（接口分解）、7.1（Naturalness）、7.11（问询的逆 / "a claim is a claim about what can be done"）
  - 声明头：[include/openkal/terminal.h](https://github.com/mcpplibs/openkal/blob/86eb855/include/openkal/terminal.h)（模式字 KAL_TERM_LINE_EDIT/ECHO，props 字 KAL_TERM_PROP_MODE/SIZE）
  - 先例：SPEC 历史条目 7（文件锁——"What was missing was a word, not a capability"）、7.11（`kal_fs_set_modified` 的准入判据——"three ordinary programs could not be written above the interface without it"）
  - 本地方程式测量：issue #36 的两组对照程序（musl 构建 vs glibc 构建，同一 tmux server）

---

## 0. 一句话

**模式字加第三个位 `KAL_TERM_CONTROL`（环境是否消费控制键），配套 props 位 `KAL_TERM_PROP_CONTROL`；三平台本机开关一一对应（ISIG / ENABLE_PROCESSED_INPUT），缺的只是词汇不是能力；openkal 先发 0.14，index 注册后四个下游仓库按依赖序适配。**

---

## 1. 问题与消费者需求

### 1.1 需求（与信号模型无关）

全屏/逐键程序的真实需求只有一句：**"我读的每个字节就是用户按的每个键"**，包括 0x03。分三种场景：

| 场景 | LINE_EDIT | ECHO | CONTROL | 例子 |
| --- | --- | --- | --- | --- |
| 全屏程序 | 0 | 0 | **0**（控制键也是数据） | vim、less、游戏 |
| 自带行编辑的程序 | 0 | 0 | **1**（^C 仍"打断"） | bash/readline：关 ICANON 但保留 ISIG |
| 普通程序 | 1 | 1 | 1（默认 cooked） | 一切照旧 |

第二种场景是硬约束：**它否决了"把中断语义折进 LINE_EDIT=0"的偷懒方案**，所以 CONTROL 必须是独立的正交位。

### 1.2 三平台本机语义

| 抽象 | Linux (termios) | macOS (termios) | Windows console |
| --- | --- | --- | --- |
| 行组装 | `ICANON` | `ICANON`（同 Linux，POSIX） | `ENABLE_LINE_INPUT` |
| 回显 | `ECHO` | `ECHO` | `ENABLE_ECHO_INPUT` |
| **环境消费控制键** | `ISIG`（VINTR/^C 等） | `ISIG` | `ENABLE_PROCESSED_INPUT`（^C→CTRL_C_EVENT，另含 ^S/^Q） |

两个观察：macOS 与 Linux 完全同构（不是三种语义而是两种）；Windows 恰好存在一一对应的开关。三个目标环境都有现成、几乎拼写一致的承载机制——这正是 SPEC 历史条目 7（文件锁）的准入形状："every environment this specification targets locks a byte range, and spells it almost identically. What was missing was a word, not a capability."

---

## 2. 设计方案

### 2.1 声明（将加入 `include/openkal/terminal.h`）

```c
#define KAL_TERM_CONTROL      ((kal_uintptr)1u << 2)  /* 模式字位 2 */
#define KAL_TERM_PROP_CONTROL ((kal_uintptr)1u << 2)  /* props 字位 2 */
```

语义：

- **CONTROL=1（默认）**：环境可保留约定控制键用于自身动作。Linux/macOS 映射为 `ISIG`，Windows 映射为 `ENABLE_PROCESSED_INPUT`。openkal 不承诺"信号"机制（SPEC.md:284-304 明确排除），只承诺"环境会行动"；在 musl 端口下对程序的可见效果就是默认处置（终止），与端口"无 handler"的既定立场自洽。
- **CONTROL=0**：**保证**每个键以字节到达，包括 0x03。这是全屏程序在 openkal 上存活于 ^C 的唯一途径（openkal 装不了 signal handler）。
- `kal_terminal_get_mode/set_mode` 做本机映射；未分配的位仍按条款 6.2 读 0、set 忽略——**但本位置是例外，见 §2.2**。

### 2.2 条款 6.2 的字面例外（本设计唯一需要规范性文字的地方）

条款 6.2 承诺"未分配位读 0，因此对旧实现的向后兼容成立"。对绝大多数位成立（0 = 无此能力），但 CONTROL 位**不成立**：已发布的 openkal-linux 0.12/0.13 不认此位（get_mode 读 0 = "环境不行动"），而其实际行为是保留 ISIG（等效 CONTROL=1）。新规范程序对旧实现 set CONTROL=0 会被静默忽略，程序仍被 ^C 杀死——正是 6.2 承诺要防的事故形状。

规范的两条落笔：

1. props 字加 `KAL_TERM_PROP_CONTROL`：实现是否区分该模式位。
2. 规范性例外（写入条款 6.2 的相应处）：**程序若依赖"键入皆数据"的保证，须先查 `kal_terminal_props`，不能只信 get_mode 的读 0**——因为恰好对这个位，未分配不等于行为等于 0。引用条款 7.11 的既有原则收口："An implementation that claims the position is required to perform the operation: clause 6.2 exists so that a claim is a claim about what can be done."

### 2.3 命名

模式位命名 `KAL_TERM_CONTROL`，不叫 INTERRUPT/SIGNAL：条款 7.5 已占用 "Interruption"（实现须重试被打断的操作且不得上报），含义完全不同；且 openkal 无信号模型，位名描述**环境行为**（"消费控制键"），与 LINE_EDIT/ECHO 的命名风格一致。

### 2.4 逐条符合性核对（对照 SPEC）

| 条款 | 结论 |
| --- | --- |
| 7.1 Naturalness | 符合。三实现直接映射本机开关，无需翻译表/注册层 |
| 6.4 接口分解 | 符合。语义只在交互流上存在，落在既有 `openkal.terminal`，不开新接口 |
| 7.11 问询的逆 / 准入判据 | 符合。全屏编辑器是"C 库之上预期托管的程序"，无此位无法正确写出——正是 `kal_fs_set_modified` 的准入形状 |
| 6.2 版本化 | 基本符合，配 §2.2 的 props 位 + 规范性例外 |
| 3.2 核心集不扩张 | 不冲突。`openkal.terminal` 是可选层接口，加位属条款 8 的标准演化 |

---

## 3. 对使用侧的效果（零改动）

- musl 的 `cfmakeraw` 本来就清 ISIG。openkal-musl 把 ioctl 分支接到 `kal_terminal_*` 后，`tcgetattr` 读 CONTROL→ISIG、`tcsetattr` 写 ISIG→CONTROL，**现有全屏程序不改动即正确**。
- readline 类程序"关 ICANON、留 ISIG"的写法映射为 CONTROL=1，语义不变。
- 依赖链上唯一要改变行为的是 openkal-linux 的 `set_mode`：目前清 LINE_EDIT 时保留其余 lflag（read-modify-write），产生 issue #36 指出的"半 raw 被杀"状态。有了 CONTROL 位后按模式字逐位置写——这本来就是 get/set 对称性（terminal.h:16-20）的要求。

---

## 4. 非目标（out of scope）

- **不改写 openkal 的"无信号"模型**（SPEC 排除信号接口是既定设计，KAL_TERM_CONTROL 不引入任何信号形状）。
- **不在本设计中修 openkal-musl 的 SIG_IGN 静默 no-op**（okm_syscall.c 接受 SIG_IGN 却不安装）。它与本设计无关，是独立的端口 bug，遵循规范"接受⇒生效，否则拒绝"原则，应作为 openkal-musl 侧独立修复（可同批 PR 也可单独）。
- **不要求 openkal-musl 支持 signal handler**（ENOSYS 是端口对"无内存保护"立场的既定实现）。

---

## 5. 任务分解与依赖关系

```
                 ┌─────────────┐
                 │ 1. openkal   │  规范 PR（terminal.h 两个宏 + 条款 6.2 例外
                 │  spec 0.14   │  + SPEC 历史条目 + mcpp.toml/README 版本）
                 └──────┬───────┘
                        │ 合并 + 打 tag 0.14.0
                        ▼
                 ┌─────────────┐
                 │ 2. mcpp-index│  注册 openkal 0.14.0 描述符
                 └──────┬───────┘
            ┌───────────┼───────────────┐
            ▼           ▼               ▼
   ┌────────────┐ ┌────────────┐ ┌────────────┐
   │3a.openkal- │ │3b.openkal- │ │3c.openkal- │  三实现并行，互不依赖；
   │  linux     │ │  windows   │ │  macos     │  各自 bump 依赖 openkal=0.14.0
   └─────┬──────┘ └─────┬──────┘ └─────┬──────┘
         └──────────────┼──────────────┘
                        ▼
                 ┌─────────────┐
                 │4. openkal-musl│  bump 依赖 openkal-linux 0.14.x；
                 │              │  ioctl 分支路由到 kal_terminal_*，
                 │              │  ISIG↔CONTROL 映射；port 自测（pty 探针）
                 └─────────────┘
```

**为什么是这个序：**

1. **openkal 先行（唯一硬序）。** 下游四仓库的适配代码都要 `#include <openkal/terminal.h>` 里的新宏；mcpp 依赖解析也要求实现包的 `openkal = "0.14.0"` 依赖先存在于 index。规范 PR 合并且 tag 后才能动下游。
2. **mcpp-index 第二。** 实现包 bump 依赖到 openkal 0.14.0 后，其 CI/构建需要 index 里已注册该版本描述符，否则依赖解析失败。
3. **三个实现并行（无相互依赖）。** linux 与 macos 都是 termios（改动几乎同形），windows 是 console mode；三者只共同依赖规范，彼此独立，PR 可同时开。
4. **openkal-musl 最后。** 它依赖 `openkal-linux`（及其 macos/windows 对应物）的新实现来跑端口级验证；且它的改动最大（ioctl 分发器重写 + 链接接线），放在实现稳定之后风险最低。

### 5.1 任务卡

| # | 仓库 | 内容 | 验收 |
| --- | --- | --- | --- |
| 1 | openkal | terminal.h 加 `KAL_TERM_CONTROL`/`KAL_TERM_PROP_CONTROL`（含房屋风格理由注释）；src/terminal.cppm、src/macros.cppm 镜像；SPEC.md：版本 0.14、条款 6.2 规范性例外、历史条目；mcpp.toml/README 0.14.0；conformance terminal 段加观察（props 宣布则 roundtrip 该位并恢复） | tools/check-*.sh 全绿；CI 四声明矩阵绿 |
| 2 | mcpp-index | 注册 openkal 0.14.0 描述符（仿 register-openkal-0.15.0 分支既有流程） | index CI 绿 |
| 3a | openkal-linux | `kal_terminal_get_mode` 读 ISIG→CONTROL；`set_mode` 按模式字逐位置写 ISIG（不再"保留其余"）；`kal_terminal_props` 宣布 KAL_TERM_PROP_CONTROL | 端口自测 + openkal conformance 通过 |
| 3b | openkal-windows | get/set `ENABLE_PROCESSED_INPUT`；props 宣布 | 同 3a（Windows leg） |
| 3c | openkal-macos | 同 3a（termios） | 同 3a（macOS leg） |
| 4 | openkal-musl | okm_syscall.c ioctl 分支：TCGETS/TIOCGWINSZ 把真实 termios/winsize 拷回调用者（不再"成功但不写"）；TCSETS 族路由 `kal_terminal_set_mode`；tcgetattr 读回 ISIG↔CONTROL 映射使 `cfmakeraw` 免费正确；bump 依赖 openkal-linux 0.14.x | issue #36 的两支探针程序在 pty 下行为与 glibc 对照一致 |

### 5.2 版本链

`openkal 0.14.0` → index 注册 → `openkal-linux/windows/macos` 各自 minor bump（依赖 `openkal = "0.14.0"`）→ index 注册 → `openkal-musl` minor bump（依赖新实现）。KAL_VERSION_MINOR 不动（仓库惯例：0.12/0.13 发布均未 bump version.h）。

---

## 6. 风险与开放问题

- **R1：已发布实现的语义债。** openkal-linux 0.12/0.13 的旧行为（清 LINE_EDIT 保留 ISIG）在新规范下应视为 bug：它的 `set_mode` 必须先升级才能诚实地声称 KAL_TERM_PROP_CONTROL。3a 的 PR 即还债。
- **R2：conformance 的可见性。** "^C 以字节到达"这一核心保证需要 pty + 按键注入，CI 终端环境下难以自动观察；折衷是只观察 props 宣布 + 位 roundtrip，端到端保证留给 issue #36 的探针程序在 openkal-musl 侧人工/脚本验证（已列入 4 的验收）。
- **O1（可选，留给规范作者）**：是否在历史条目里同时记录"SIG_IGN 静默 no-op"作为 openkal-musl 侧独立修复的锚点——本文 §4 将其排除在设计外，但两 PR 同批走可减少消费者等待。
