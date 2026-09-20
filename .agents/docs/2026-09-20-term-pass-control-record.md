# KAL_TERM_PASS_CONTROL：本波的记录

- 日期：2026-09-20
- 设计：`2026-09-20-kal-term-pass-control-design.md`；计划：`2026-09-20-term-pass-control-execution-plan.md`；对前一方案的 review：`2026-09-20-pr37-term-control-review.md`
- 起因：消费者报告 [mcpplibs/openkal-musl#36](https://github.com/mcpplibs/openkal-musl/issues/36)
- 分支名（各仓库一致，CI 据此检出同名规范分支）：`openkal-0.14`

---

## 1. PR 与版本

| 仓库 | PR | 版本 | 内容 |
| --- | --- | --- | --- |
| openkal | [#38](https://github.com/mcpplibs/openkal/pull/38) | 0.13.0 → 0.14.0 | `KAL_TERM_PASS_CONTROL`；条款 6.2 的极性规则；`set_mode` 两条规则；§11 条目 20；`version.h` 0.14.0 与 `tools/check-version.sh`；套件新观察 |
| openkal-linux | [#28](https://github.com/mcpplibs/openkal-linux/pull/28) | 0.13.0 → 0.14.0 | `ISIG|IXON|IEXTEN` 映射；`VMIN=1/VTIME=0`；自带测试新增观察 |
| openkal-macos | [#22](https://github.com/mcpplibs/openkal-macos/pull/22) | 0.10.0 → 0.11.0 | 同上，本内核常量 |
| openkal-windows | [#25](https://github.com/mcpplibs/openkal-windows/pull/25) | 0.8.0 → 0.9.0 | `ENABLE_PROCESSED_INPUT`，取反 |
| openkal-emscripten | [#3](https://github.com/mcpplibs/openkal-emscripten/pull/3) | 0.2.0 → 0.3.0 | 转发宿主 termios；absence 检查改为取工作树，engine pin 升到 index 下限 |
| openkal-uefi | [#14](https://github.com/mcpplibs/openkal-uefi/pull/14) | 0.7.0 → 0.8.0 | 跟随规范（不提供 terminal，跟的是 `kal_version`） |
| openkal-opensbi | [#17](https://github.com/mcpplibs/openkal-opensbi/pull/17) | 0.7.0 → 0.8.0 | 同上 |
| openkal-musl | [#38](https://github.com/mcpplibs/openkal-musl/pull/38) | 0.15.0 → 0.16.0 | ioctl 路由到 `openkal.terminal`；信号处置只接受已生效的；`examples/terminal` + `tools/pty-keys.py` |
| openkal-llvm-runtime | [#25](https://github.com/mcpplibs/openkal-llvm-runtime/pull/25) | 0.11.0 → 0.12.0 | 跟随 musl 0.16.0（并把停在 main 的 0.11.0 带回） |
| mcpp-index | 待填 | — | 注册以上全部描述符 |

## 2. 设计落点（与设计稿的差异）

1. **位名**定为 `KAL_TERM_PASS_CONTROL`（设计稿备选 `KEYS_AS_DATA`/`PASS_KEYS`）。
2. **条款 6.2 的那一句写了**（设计稿 R5 留待裁定）：位按「零是弱主张」的方向拼写，并说明这条规则是在指派这个位时被认出来的。
3. **`VMIN`/`VTIME` 与版本修正同批**（设计稿 R4 留待裁定），理由见计划 §0 S3、S4。
4. **一个位压缩三个机制的代价**写进了头文件、SPEC §11 条目 20 与实现注释（计划 §0 S1）。
5. **端口的映射方向不对称**：读取时三机制全清才报告，写入时只看 `ISIG`。设计稿未区分这两个方向，这是落地时发现的：要求三者全清才转达，会让 `cfmakeraw` 能用而「只清 ISIG」的程序静默无效——正是本报告的缺陷形状。

## 3. 验证

### 3.1 规范内

- `tools/check-version.sh`（新增）通过，并且 CI 证明它会拒绝漂移；负向探针在三个系统上都能跑（`sed -i` 的不可移植写法已改）。
- `check-declarations.sh` 104 names、`check-types.sh` 175 declarations、`gen-macros.sh` 重生成无 diff、`mcpp build` 通过。

### 3.2 一致性套件

对 openkal-linux（本波分支），在**带尺寸的伪终端**中运行：

```
observations: 199 held, 0 did not hold, 3 not observed
kal_version = 0xe0000
openkal.terminal
  held  an interactive stream reports its mode
  held  setting the mode that was read succeeds
  held  the mode read back is the mode that was set
  held  an unassigned position in the mode word is not an error
  held  asking that every keystroke be passed on is not an error
  held  the position is either distinguished or reads as zero
  held  the mode the section found is the mode it leaves
  held  a reported display size is not zero in either dimension
```

在没有窗口尺寸的伪终端里，`a reported display size is not zero` 不成立——这是探测装置而不是实现：`pty.fork` 不设置窗口大小时，内核报告 0x0。

### 3.3 端口级（本波的核心证据）

`examples/terminal` 在伪终端中，与同一份源码在宿主 C 库上的对照，去掉回车后逐行相同：

```
isatty 1
tcgetattr rc=0 errno=0
before   lflag_icanon=1 lflag_echo=1 lflag_isig=1 iflag_ixon=1 vmin=1 vtime=0
tcsetattr rc=0 errno=0
tcgetattr(readback) rc=0 errno=0
readback lflag_icanon=0 lflag_echo=0 lflag_isig=0 iflag_ixon=0 vmin=1 vtime=0
reading
byte 0x61
byte 0x62
byte 0x03
byte 0x71
restored lflag_icanon=1 lflag_echo=1 lflag_isig=1 iflag_ixon=1 vmin=1 vtime=0
done
```

对照 issue #36 里同一程序在本端口上的旧行为：`tcsetattr rc=-1 errno=25`，`before lflag=0x5a5a5a5a`（哨兵未被覆盖），以及 `ab^C` 之后 `Pane is dead (signal 2)`。

**比较时去掉回车，排除的是一件已记录的事**：openkal 的模式字管「键入」，不管「写出」，所以输出后处理（`OPOST`/`ONLCR`）在本端口的 `tcsetattr` 下不受影响，而宿主 C 库的 `cfmakeraw` 会清掉它。两份 transcript 因此每行差一个字节，其余完全一致。这条限制写进了 openkal-musl 的 README 限制表，并作为下一波的候选问题记在设计稿 §7 O2。

### 3.4 生态级

（待填：发布、index 注册、沙箱解析、真实 TUI 程序 `re-cloud-code` 的构建与运行。）

## 4. 本波修掉的、与设计无关但同源的缺陷

- **`KAL_VERSION_MINOR` 停在 11**：整个生态的 `kal_version` 都答 0.11，条款 6.2 给 load 期绑定消费者的比较形同虚设。修正并加了检查。
- **openkal-musl 的三处「报告成功却什么都没做」**：`TCGETS`、`TIOCGWINSZ` 不回写调用者结构，`SIG_IGN` 不安装。前两者由本波的 ioctl 路由一并解决，第三者改为「只接受已经生效的处置」。
- **openkal-emscripten 的 absence 检查依赖 index**：它按本仓库清单里的版本号去 index 取规范，于是在提升版本的分支上必然失败，并把失败报成「链接没有提到 kal_process_spawn」。改为两个依赖都取工作树。
