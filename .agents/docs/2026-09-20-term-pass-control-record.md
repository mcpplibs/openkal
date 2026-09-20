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
| mcpp-index | [#442](https://github.com/mcpplibs/mcpp-index/pull/442) | — | 一次注册九个描述符；顺带补上 `main` 上缺的两处 `xpm.macosx` |

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

### 3.4 生态级：一个真实的全屏程序

`/home/speak/workspace/scode/re-cloud-code` 是报告这个问题的项目——一个 C++ 全屏 TUI，`packages/tui-kit/src/term.cppm` 里正是 `tcgetattr` / `cfmakeraw` / `tcsetattr` 加 `sigaction(SIGINT)` 的写法，源码注释里还留着「openkal-musl 的 tcsetattr 答 ENOTTY」这一句。

在本波的链上构建（`openkal-linux@0.14.0` · `openkal-musl@0.16.0` · `openkal-llvm-runtime@0.12.0`，目标 `x86_64-linux-musl`），在带尺寸的伪终端里运行：

| 观察 | 结果 |
| --- | --- |
| 程序进入全屏并绘制 | 是（边框、颜色、状态行都在） |
| 键入的文本到达程序 | 是——输入框里出现 `❯ hello openkal` |
| 按下 `^C` 之后程序还在 | 是，且它把 `^C` 当作「清空输入」处理，而不是被结束 |

对照 issue #36：同一类程序在旧链上 `tcsetattr rc=-1 errno=25`，然后 `Pane is dead (signal 2)`。

这一项先用本地工作树（path 依赖）测了一遍，因为它跑在 index 注册之前；注册之后又按**只写版本号**的方式重做了一遍——十六个清单里只有一行 `openkal-llvm-runtime = "0.12.0"`，其余不变，重新构建后同样的两项观察：键入的文本到达程序、按下 `^C` 之后程序还在。消费者要做的就是这一行。

### 3.5 沙箱：只写版本号，从已发布的 index 解析

`xlings subos use term014 --sandbox --cmd "… bash 2026-09-20-term-pass-control-verify.sh"`，CN 镜像，mcpp 2026.9.18.3：

```
== A. identity and mirror ==
ok: mcpp 2026.9.18.3
ok: xlings mirror is CN

== B. openkal 0.14.0 resolves and states its version ==
ok: header 0.14.0 implementation 0.14.0 pass_control 4

== C. the position is reachable as a macro and as a module constant ==
ok: kal::terminal::pass_control and KAL_TERM_PASS_CONTROL_M are the same position

== D. the interrupt keystroke arrives as data above openkal-musl 0.16.0 ==
ok: the transcripts agree, keystroke for keystroke

0 assertion(s) failed
```

B 这一行是版本修正的可观察形式：实现自述 0.14.0，而在本波之前每个实现都答 0.11.0。

两件在这一步才暴露的事，都已修：

1. **index 是按构件分发的，不是按 main 分支。** 合并之后 `Publish Index Artifact` 工作流把 `pkgs/**` 发成 `xlings-res/mcpp-index@v<sha>`，客户端同步到的是那个构件；在它发完之前，沙箱解析 0.14.0 会报「not found in the synced index (mcpplibs@artifact:8d87b18)」。等构件发布并 `mcpp index update` 之后一次通过。
2. **脚本里的 pty 辅助文件不能叫 `pty.py`。** Python 把脚本自己所在目录放在导入路径最前面，于是它遮蔽了标准库的 `pty`，报 `AttributeError: partially initialized module 'pty' has no attribute 'fork'`——而脚本把这次失败归到了「本机 C 库也没收到这个按键」，即把自己的缺陷说成了对照组的。改名为 `ptykeys.py`。

## 4. 本波修掉的、与设计无关但同源的缺陷

- **`KAL_VERSION_MINOR` 停在 11**：整个生态的 `kal_version` 都答 0.11，条款 6.2 给 load 期绑定消费者的比较形同虚设。修正并加了检查。
- **openkal-musl 的三处「报告成功却什么都没做」**：`TCGETS`、`TIOCGWINSZ` 不回写调用者结构，`SIG_IGN` 不安装。前两者由本波的 ioctl 路由一并解决，第三者改为「只接受已经生效的处置」。
- **openkal-emscripten 的 absence 检查依赖 index**：它按本仓库清单里的版本号去 index 取规范，于是在提升版本的分支上必然失败，并把失败报成「链接没有提到 kal_process_spawn」。改为两个依赖都取工作树。

## 5. 发布

| 包 | 版本 | tag | sha256（GitHub archive 与 GitCode 资产逐字节相同） |
| --- | --- | --- | --- |
| openkal | 0.14.0 | `0.14.0` | `0e0410fbda5f246c79e45ff157c919e23173b3a8a12a90a9538aeeacea1e692c` |
| openkal-linux | 0.14.0 | `0.14.0` | `331a6524a4ff604a208330f45c5b9f0ea17943f4783523a6170444341a27ef45` |
| openkal-macos | 0.11.0 | `0.11.0` | `576356ead9a99bb299d9d8ac6e7d266064112cff1775252935922e67e6a45809` |
| openkal-windows | 0.9.0 | `0.9.0` | `27122bef192b14998ba603ad54031b8c454e0ca372ce907ed5487fe5f7f33d02` |
| openkal-emscripten | 0.3.0 | `0.3.0` | `34067d2ac9011344cd6cdf6f4866d771ba49b5e05d392bb0d51f573f31fe6606` |
| openkal-uefi | 0.8.0 | `0.8.0` | `a1c920dc862a974431b7c8518cdb8085bc6d0ff22bbacbd4d97fe7f8b16720ac` |
| openkal-opensbi | 0.8.0 | `0.8.0` | `557a28f7d775871df7dad68422617d9bbcd844e8bb2be190f38fe87aab05e693` |
| openkal-musl | 0.16.0 | `0.16.0` | `8ffa4a2a79fcc7fe7565c1e69624b9d3b575020ed97d2043e20cdf542d519105` |
| openkal-llvm-runtime | 0.12.0 | `0.12.0` | `e009f6195ef517c40beaf5093cde59764fc870502aab5967f5a2817f8df47cff` |

mcpp-index PR [#442](https://github.com/mcpplibs/mcpp-index/pull/442) 一次注册以上九个描述符。

### 5.1 发布惯例上发现的一处缺陷（上一波留下的，本波已修好）

`openkal-musl 0.15.0` 与 `openkal-llvm-runtime 0.11.0` 在 GitHub 上的标签带 `v` 前缀，而描述符的 GLOBAL 地址按惯例写无前缀形式，**这两个版本的 GLOBAL 地址曾返回 14 字节的 Not Found**。只补一个无前缀标签修不好：GitCode 上的资产不是 GitHub archive 本身（`75803192…` 对 `ee953bd8…`），而一个版本只有一个 sha256，所以无论地址怎么写两边都不可能同时对。

**改源头而不是改描述符**：仓库所有者在 GitCode 侧删除了那两个资产之后，两个仓库各补了指向同一提交的无前缀标签，该标签的 GitHub archive 用 `gtc` 上传顶替原资产，描述符写这同一个文件的哈希。下载两边比对：`openkal-musl 0.15.0` = `ee953bd8…`、`openkal-llvm-runtime 0.11.0` = `b9b8eddb…`，GLOBAL 与 CN 逐字节相同。mcpp-index PR [#443](https://github.com/mcpplibs/mcpp-index/pull/443)。

修的过程中 index 自己的 CN 检查抓到一处附带损伤：`openkal-llvm-runtime 0.1.1` 的 GitCode 资产也不在了。用 GitHub 上同标签的 archive 补回，它的哈希正是描述符里原本写的那个（`d3c460e0…`），所以描述符不动。随后把九个 openkal 包的 135 条 CN 地址全扫了一遍，全部 200。

本波九个包都按惯例发布：无前缀标签，GitCode 资产是 GitHub archive 原样上传，两边 sha256 相同（openkal 0.14.0 下载两边比对确认）。

## 6. 生态级自我 review

按本波计划 §1 的八个角度逐条回答，附上没有做到的那几项。

| 角度 | 结论与证据 |
| --- | --- |
| 架构 | 净增一个模式位、一条条款 6.2 的规则、两条实现规则、一条 §11 记录。不新增接口、不新增操作、不动结构布局、不动 props 字 |
| 稳定性 | 旧实现读回 0 仍是真话，因此没有「先还债才能诚实」的前置；index 旧描述符原样保留，0.13 消费者不受影响 |
| 优雅简洁 | 与被取代的方案相比，少了一条条款 6.2 的例外与一个强制 props 位；发现机制用的是 terminal.h 已承诺、套件已观察的 get/set 往返 |
| 用户体验 | 消费者零改动：`tcgetattr` / `cfmakeraw` / `tcsetattr` 的既有写法在真实 TUI 上直接正确（§3.4） |
| 兼容性 | 0.14 程序在 0.13 实现上读回 0 并可降级；版本号变真之后，用旧实现配新规范的组合会被套件判为过旧——这正是 version.h 那段话要的行为 |
| 跨平台 | 四个实现各自映射本机机制的**全集**；没有线路规程的环境恒报 1 并忽略置 0 |
| 一致性 | 规范文字不提 termios、不提 console；端口层的映射方向不对称，理由写在端口里 |
| 无感升级 | 九个包按依赖序发布并一次注册；消费者改一行版本号 |
| 测试覆盖 | 见下 |

**测试覆盖的实情，包括没有覆盖到的部分。**

- Linux 腿：端到端全覆盖——套件 199 held（带尺寸 pty）、端口探针与宿主 C 库逐行一致（CI 每次运行）、真实 TUI 程序在 pty 中键入与 `^C` 都正确。
- macOS / Windows / emscripten 三条腿：编译、链接、套件都通过，但**CI 没有伪终端**，所以 `openkal.terminal` 段在这三条腿上报告为「未观察」。新位在它们上面是「按同一份源码形状映射、按同一套声明编译」而不是「被观察到」。
- 可做而本波没做的下一步：macOS 的 CI 有 `script`，`OPENKAL_CONFORMANCE_RUNNER="script -q /dev/null"` 就能把套件放进伪终端里跑（`tools/run-conformance.sh` 里 `$runner "./$binary"` 是不加引号展开的，本就为此留着）。Windows 需要一个控制台宿主，代价更高。

  **但现在打开它会立刻红，而红的原因是另一件事。** 没有父终端的 CI 里，`script` 开出来的伪终端窗口尺寸是 0x0；openkal-linux 与 openkal-macos 把这对零原样报成一个尺寸并返回 `kal_ok`，于是套件的「a reported display size is not zero in either dimension」不成立。openkal-emscripten 对同一情况的处理相反，它的注释写着 "A ZERO DIMENSION IS NOT A SIZE"，报 `kal_err_not_supported`。**三个实现对「尺寸是零」的答法不一致，而规范没有就此落过字**：本地用 `pty.fork` 不设尺寸跑套件就能重现（本波 §3.2 记的那一次）。

  这项缺陷早于本波，影响面小（只在没有尺寸的伪终端上），修法也小（`terminal.h` 一句 + 两个实现各两行），但要动三个包的版本，所以记在这里留给下一波，和「打开 macOS 的伪终端行」一起做。

**其余已知且记录在案的限制**：输出后处理（`OPOST`）不在模式字内，raw 模式下写 `\n` 仍会先得到回车；混合保留态恢复为环境惯常的集合；`VMIN`/`VTIME` 不可表达，要「会放弃的读」用 `kal_timeout_read`；openkal-musl 0.15.0 与 openkal-llvm-runtime 0.11.0 的镜像不一致（上一波留下）已在本波修好，见 §5.1。
