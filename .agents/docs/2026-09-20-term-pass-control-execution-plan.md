# KAL_TERM_PASS_CONTROL：执行计划与生态闭环

- 日期：2026-09-20
- 依据：`.agents/docs/2026-09-20-kal-term-pass-control-design.md`（v2 设计稿）
- 原则：每个仓库一个 PR，实现全部方案；分支名在各仓库一致（`openkal-0.14`），因为各仓库 CI 用 `github.head_ref` 去检出同名的规范分支；文档与注释不含表情符号。

---

## 0. 自我 review：设计稿在落地前需要补的四条

**S1 模式字压缩是有损的，损在哪里必须写下来。** `PASS_CONTROL` 一个位对应 termios 上三个机制（`ISIG`、`IXON`、`IEXTEN`）。一个终端若本来就处在"部分释放"的混合态（用户 `stty -ixon` 是常见配置），`get_mode` 只能报 0，程序 raw 一趟再恢复，`IXON` 会被重新打开——"用户回到的终端不是他原来那个"。三个位分列可以无损，但 Windows 只有一个开关（`ENABLE_PROCESSED_INPUT` 同时管中断与流控），分列会要求 Windows 实现它拿不出的区分，条款 6.4 判否。**结论：保留一个位，把这项代价写进 SPEC §11 条目与头文件注释**（体例照条目 9 的 "What this costs"）。

**S2 为减小损面，补一条实现规则：请求值与当前值相同时，不得改动该位覆盖的任何机制。** 单机制的位（`LINE_EDIT`、`ECHO`）上这条自动成立；多机制的位上它决定了"只改回显的程序不会碰到流控"。写在 `set_mode` 旁。

**S3 `VMIN`/`VTIME` 的那一条是必须的，理由比设计稿说得更强。** openkal-musl 把 `tcsetattr` 映射到模式字之后，`cfmakeraw` 写下的 `VMIN=1/VTIME=0` **会被丢弃**——没有位承载它。若终端停在 `VMIN=0`，`kal_stream_read` 返回 0，而条款 7.4 说"零即输入结束"。所以这不是"顺带做的好事"，是本设计不做就会**造出**的缺陷。

**S4 版本号变真本身是一次行为变化。** `conformance/src/sections/version.cpp` 已经在断言 `kal_version() >= kal::header_version`。今天整个生态的实现都报 0.11（`kal_version` 就是 `return KAL_VERSION;`，而 `version.h` 停在 11），断言恒真。把 `version.h` 改到 0.14.0 之后，**未随本波重建的实现会被 0.14 的套件判为过旧**——这正是该断言的本意。因此版本修正必须与六个实现在同一波里落地，且 `mcpp-index` 里旧描述符保持不动（旧消费者按旧版本对解析，仍是自洽的一对）。

---

## 1. 多角度判据

| 角度 | 本波的判据 |
| --- | --- |
| 架构 | 不新增接口、不新增操作、不改结构布局；语义落在既有 `openkal.terminal` 的模式字 |
| 稳定性 | 旧实现在新规范下说的仍是真话（极性选择的直接结果），无语义债、无强制升级 |
| 优雅简洁 | 规范净增：一个模式位、条款 6.2 一句规则、§11 一条记录；不加 props 位、不加例外 |
| 用户体验 | 消费者侧零改动：`tcgetattr`/`cfmakeraw`/`tcsetattr` 的既有写法自动正确；失败可被读回发现 |
| 兼容性 | 0.13 消费者不受影响；0.14 程序在 0.13 实现上读回 0 并降级；index 旧描述符保留 |
| 跨平台 | 四个实现各自映射本机机制；无线路规程的环境恒报 1 并忽略置 0，诚实且无需模拟别人的键表 |
| 一致性 | 位的措辞不提任何环境的机制名；四实现映射各自机制的**全集**，避免 `^S` 在 Linux 冻终端而 Windows 不冻 |
| 无感升级 | 版本链一次走完（规范 → index → 四实现 → C 环境 → 运行时）；消费者只改一行版本号 |
| 测试覆盖 | 套件内：位的往返、未分配位不报错、props 与操作一致；套件外：pty 探针（`^C` 以 0x03 到达、`^S` 不冻结、读不返回假零）；生态级：真实 TUI 程序在沙箱里跑 |

---

## 2. 仓库任务卡

| # | 仓库 | 版本 | 内容 | 验收 |
| --- | --- | --- | --- | --- |
| 1 | `openkal` | 0.13.0 → **0.14.0** | `terminal.h` 新位与语义、`set_mode` 的两条实现规则（S2、S3）；`src/terminal.cppm` 的 `pass_control`；`tools/gen-macros.sh` 重生成；SPEC 条款 6.2 一句 + §11 条目 20；`version.h` 0.14.0 + `tools/check-version.sh`；conformance 新观察；README/mcpp.toml 版本 | `tools/check-*.sh` 全绿；conformance 对 openkal-linux 全绿；CI 全绿 |
| 2 | `openkal-linux` | 0.13.0 → **0.14.0** | `mode_of` 读 `ISIG|IXON|IEXTEN`；`set_mode` 按位写全集 + `VMIN=1/VTIME=0`；依赖 0.14.0 | 自带测试 + pty 探针；conformance 全绿 |
| 3 | `openkal-macos` | 0.10.0 → **0.11.0** | 同 2（termios 同构，常量值不同） | 同 2（macOS leg） |
| 4 | `openkal-windows` | 0.8.0 → **0.9.0** | `ENABLE_PROCESSED_INPUT` 的读写；依赖 0.14.0 | 同 2（Windows leg） |
| 5 | `openkal-emscripten` | 0.2.0 → **0.3.0** | 转发宿主 termios 的同一映射；依赖 0.14.0 | 同 2（node leg） |
| 6 | `openkal-uefi` | 0.7.0 → **0.8.0** | 跟随规范 0.14.0（不提供 terminal，只改依赖与文档） | CI 全绿 |
| 7 | `openkal-opensbi` | 0.7.0 → **0.8.0** | 同 6 | CI 全绿 |
| 8 | `openkal-musl` | 0.15.0 → **0.16.0** | ioctl 分发器路由到 `kal_terminal_*`：`TCGETS`/`TCSETS` 族真实读写、`TIOCGWINSZ` 真实回填；`ISIG|IXON|IEXTEN` ↔ `PASS_CONTROL`；`rt_sigaction` 的 `SIG_IGN` 不再假成功 | issue #36 的两支探针与宿主 C 库对照一致 |
| 9 | `openkal-llvm-runtime` | 0.11.0 → **0.12.0** | 跟随 openkal-musl 0.16.0 | CI 全绿 |
| 10 | `mcpp-index` | — | 注册以上全部新版本描述符 | index CI 绿；沙箱能只写版本号解析 |

---

## 3. 依赖与顺序

```
1 openkal 0.14.0 ──tag──> mcpp-index 注册
        ├──> 2 linux ──┐
        ├──> 3 macos   ├── 并行，互不依赖 ──> mcpp-index 注册
        ├──> 4 windows │
        ├──> 5 emscripten ┘
        ├──> 6 uefi / 7 opensbi（并行，跟随）
        └──> 8 openkal-musl（依赖 2..5 的实现）──> 9 llvm-runtime ──> 10 index
                          └──> 生态验证：xlings 沙箱 + re-cloud-code 真实 TUI
```

唯一硬序是规范先行：下游都要 `#include <openkal/terminal.h>` 的新位，且依赖解析要求 `openkal = "0.14.0"` 先在 index 里。各仓库 PR 可同时开：CI 按分支名检出同名规范分支，因此在合并前就能看到真实结果。

---

## 4. 验证层次

1. **规范内**：`tools/check-declarations.sh`、`check-surface.sh`、`check-types.sh`、`check-readme-versions.sh`、新增 `check-version.sh`、`gen-macros.sh` 重生成无 diff。
2. **一致性套件**：`tools/run-conformance.sh` 对每个实现跑；新位的往返观察在有终端时生效，在管道下按既有方式报告为未观察。
3. **端口级**：pty 下的探针程序（进仓库，不留在 issue 里）：`^C` 到达为 `0x03`、`^S` 不冻结、恢复后终端回到原样、读不返回假零。
4. **生态级**：`xlings subos ... --sandbox --cmd`，只写版本号解析全链；再用 `/home/speak/workspace/scode/re-cloud-code`（真实 TUI，`packages/tui-kit/src/term.cppm` 正是 `tcgetattr`/`cfmakeraw`/`tcsetattr` 的调用方）在 musl 目标上构建并运行。

## 5. 发布

每个仓库：PR 合并 → 打 tag → GitHub release → `gtc` 补 GitCode 镜像资源 → `mcpp-index` 注册描述符（sha256 记录在本波的 record 文档）。顺序同 §3。

## 6. 风险

- **R1** 版本号变真使未重建的实现被套件判为过旧（S4）：六个实现在本波内全部重建，index 旧描述符不动。
- **R2** 套件看不到端到端保证：CI 无 pty，端到端由第 3 层探针与第 4 层真实程序承担。
- **R3** 混合态的有损恢复（S1）：记录为代价，不追加位。
- **R4** openkal-musl 的 `SIG_IGN` 由"假成功"改为"按能否兑现回答"是行为变化：已在 issue #36 中被消费者明确要求，写入发布说明。
- **R5** 输出方向（`OPOST` 一类）不在模式字内，本波不动；若真实程序因此可见异常，记录为下一波的问题而不是临时加位。
