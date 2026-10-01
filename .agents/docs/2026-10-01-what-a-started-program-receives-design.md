# 被起的程序收到什么:openkal-linux#30/#31、grant 传递,以及 tinyhttps#20–22 的设计方案

日期:2026-10-01。状态:**v3,已按自我 review 修订,进入实现**(修订内容见 §12)。
v1 → v2:根据评审意见改动了以下几处:
- D1 改为"现在就把 grant 做对",并补充对功能和需求的影响分析(§3);
- 每个项目只发一次版,版本号用补丁版本避免下游连锁重发(§7);
- tinyhttps#22 改由维护者处理,用 merge 而不是 rebase;
- 定下复用贡献者 PR 时的署名规则(§6);
- 解释 D5,并增加"用 CI 验证"这个选项;
- 新增全局 review(§9)。

依据:本机的两份评审记录 `.agents/docs/reviews/2026-10-01-tls-proxy-and-spawn-descriptor-analysis{,-ci}.md`(已被 git 忽略)。本文只写结论、设计和执行顺序,证据都在那两份记录里。

---

## 0. 一页结论

1. 外部贡献者报告的 openkal-linux#30 只是一个症状。背后是**同一条原则有四处没有落实**:被起的程序应当只收到三个流和它被授予的目录。

   | 编号 | 问题 | Linux | macOS | Windows |
   |---|---|---|---|---|
   | **P** | 子进程继承基目录描述符(issue #30) | 有,0.12 起 | 无 | 无 |
   | **A** | 调用者自己继承来的、没设 CLOEXEC 的描述符原样传下去 | 有 | 有(按代码推断) | 无 |
   | **B** | grant 已在目标号上时 CLOEXEC 没清,grant 丢失 | 有,0.11 起 | 有(按代码推断) | 不适用 |
   | **C** | 顺序 dup 覆盖其他 grant 和 base/work 描述符 | 有,0.11 起 | 有(按代码推断) | 不适用 |
   | **D** | 子进程的 `kal_fs_preopen` 根本不读 grant,名字被丢掉 | 有(实测) | 有(按代码推断) | 不适用(诚实地拒绝) |

2. **D1 的结论改了:现在就把 grant 做对,不撤回。** 撤回不会弄坏任何正在工作的功能,因为 grant 现在本来就不工作。但它会让规范的能力模型(§11 第 6 条、`space.h` 的承诺)在三个桌面实现上全部无法兑现,而且"先撤回、以后再加回来"意味着同一个实现要发两次行为相反的版本,和"每个项目只发一次"冲突(§3)。设计是:**用环境变量传递带名字的 grant,并绑定到子进程的 pid**。这和 systemd 的 `LISTEN_FDS`/`LISTEN_FDNAMES`/`LISTEN_PID` 是同一个形状,是 POSIX 上传递命名描述符的原生惯例。这个形状和平台无关,Windows 将来可以原样采用(§4)。

3. **每个项目只发一次版,而且尽量不连锁。** 所有改动都没有改变任何声明,都是让实现符合**已有**的约定,所以都用补丁版本:
   - openkal(规范)0.14.1、openkal-linux 0.15.1、openkal-macos 0.12.1、tinyhttps 0.4.0(它在缺 CA 包时改为失败,行为确实变了,所以升 minor);
   - mcpp 的版本约束默认是 caret,所以 openkal-musl、openkal-llvm-runtime、openkal-windows、emscripten、uefi、opensbi **都不需要重发**。下游重新解析依赖时会自动拿到补丁版本(需要验证,见 §7)。

4. **复用贡献者的 PR,保留他们带签名的 commit。** 贡献者 Cloud_Yun 的 5 个 commit 都是 `verified=true`,没有 Signed-off-by 尾注。
   - 规则:**不 rebase、不 squash 他们的 commit**。我们的修改作为新的 commit 追加在他们的分支上,带 `Co-authored-by` 尾注。和 base 的冲突用 merge 解决。最终用 **"Create a merge commit"** 合并,这样贡献者的签名 commit 原样留在历史里。
   - 推送要用 Sunrisepeak 的 token:speak-agent 没有写权限,而 `maintainer_can_modify` 要求推送者对 base 仓库有写权限。

5. **决策状态**:
   - D2 同意(追加到 #31);
   - D3 选兼容性更好的方案(`close_range` 遇到 ENOSYS 时回退);
   - D4 进入规范,经核对符合规范(§5),以 0.14.1 的形式发布;
   - D5 已解释,并推荐新选项"在 CI 里加一个 Windows 任务来验证"(§8);
   - **新增 D6**:版本策略,即补丁版本还是 minor 版本(§7)。

---

## 1. 范围

**包括**:
- openkal 规范:SPEC 正文、`process.h` 注释、conformance 套件、`tools/run-conformance.sh`;
- openkal-linux、openkal-macos;
- openkal-windows(只加检查,不发版);
- openkal-musl(只改注释,不发版);
- tinyhttps#21/#22;
- mcpp-index 登记。

**不包括**:
- Windows 上实现 grant(保持拒绝;协议形状已经给它留了位置);
- mcpp 并行测试漏 pipe 的修复(只开 issue)。

---

## 2. 原则(规范依据)

| 依据 | 内容 |
|---|---|
| SPEC §11 第 18 条 | "A started program receives three streams, and a descriptor at any other position is not conveyed",以及 "the shape consistent with this specification is … a named grant rather than a numbered position" |
| SPEC §11 第 6 条 | "a handle not given cannot be reached";"the party that starts the program supplies a preopen that others do not have" |
| `process.h` 中的 `kal_spawn.grants` | "The directories the started program receives, read back through `kal_fs_preopen`. A count of zero starts a program with no preopens at all, which is a different thing from not asking." |
| `process.h` 中的 `kal_process_spawn` | "STARTING THE PROGRAM WITHOUT THE THING ASKED FOR IS NOT AN OPTION" |
| `space.h` | "a program that wants a child holding only what it grants uses `kal_process_spawn` with `grants` set" |
| SPEC §7.1 | 不需要转换表、注册表或名字解析器 |

**P、A、B、C、D 违反的都是上面这些已经写下来的约定。修复不需要新的声明。**

---

## 3. D1:对功能开发的影响,以及相关需求

### 3.1 依赖 grant 的需求

| 需求 | 来源 | 现在(D 存在) | 撤回 `GRANT_DIR` 后 | 现在就实现后 |
|---|---|---|---|---|
| 能力模型:子进程只持有被授予的目录 | SPEC §11 第 6 条、`space.h`、0.8 引入 grant 的初衷 | 名义上支持,实际不工作 | 三个桌面实现全都明确不支持,规范的模型在桌面上落空 | 在 Linux/macOS 上成立 |
| 起子程序时告诉它"你的工作区叫 work" | `kal_preopen.name` | 名字被丢掉 | 不可能 | 可以 |
| "一个 preopen 都不给"(count = 0) | `process.h` | 不成立,子进程总有 `cwd` 和 `/` | 只能拒绝 | 成立 |
| 在沙箱里起命令(#30 报告者的程序) | openkal-linux#30 | 靠 bwrap 加 §7.13 | 靠 bwrap 加 §7.13 | bwrap 加 §7.13,再加 grant 告诉子进程它该用哪几个目录 |
| `posix_spawn`(openkal-musl) | POSIX 没有 grant 的概念 | 不受影响,恒传 0 | 不受影响 | 不受影响 |

### 3.2 结论

- **撤回不会弄坏任何正在工作的东西**:生态里没有调用者传非空 grants。
- **但撤回会关上一个方向**:规范的能力模型在桌面上无法兑现。将来想恢复,要再发一次实现版本,翻转属性位,让依赖属性检测的消费者再改一次。
- 和"每个项目只发一次版"放在一起看,**现在就实现是更好的选择**。工作量中等:每个实现大约 120 行,外加规范和 conformance。B/C 的修复已经有原型(报告二 §6)。

**需要写明的边界**:在 POSIX 上,grant **不是隔离**。子进程依然可以自己打开 `/`。grant 给的是:一个遵守能力纪律、只用 preopen 的 openkal 程序,能确切知道自己被交了哪些目录,而且只有这些。要防住不配合的程序,还得靠环境本身(bwrap、landlock),这正是 §11 第 6 条说的"the environment's responsibility"。这一点要写进 `process.h` 注释和各实现的 README。

---

## 4. grant 传递的设计(D1 = 现在实现)

### 4.1 协议(实现内部,规范不点名)

**父进程**(`kal_process_spawn`,`grants != nullptr`):
1. 子进程分支里,先把所有 grant 源描述符以及 `b`、`w` 都用 `F_DUPFD_CLOEXEC` 移到 `≥ 3+n`(修 C),再用 dup3/dup2 放到 `3+i`(修 B,因为源已经移开,不会出现"已在目标号上"的情况)。
2. 环境中加入 `KAL_PREOPENS=1;<pid>;<n>;<fd>,<len>:<name>;…`:
   - 名字用长度前缀,可以含任意字节,含 NUL 的名字报 `kal_err_invalid`;
   - `<pid>` 用定宽字段,在 fork/clone 之后由子进程写入自己的 pid(子进程的内存是副本,可以安全修改)。
3. 调用者传进来的 envp 里如果已有 `KAL_PREOPENS`,**一律剔除**,防止伪造或沿用过期的值。
4. `grants == nullptr`(没有要求):不加这个变量,同样剔除继承来的。

**子进程**(`kal_fs_preopen` 表初始化时):
1. 有 `KAL_PREOPENS` 且 pid 等于 `getpid()`:用变量里列出的描述符和名字建表;每个 fd 先 `fstat` 确认是目录,不是就把 handle 置 0(`kal_fs_preopen` 会报 `kal_err_permission`,和现在打不开时的表现一致)。**接管时给这些 fd 加上 CLOEXEC**,这样它们不会再传给孙进程,符合 §7.13。
2. pid 不匹配:说明变量是经过一个非 openkal 的中间程序(比如 bash)传下来的,忽略它,退回到现在的 `cwd` 加 `/`。
3. `n = 0`:表为空,`kal_fs_preopen_count() == 0`,满足"count 为零就一个都没有"。
4. 没有这个变量:现在的行为不变(`cwd` 加 `/`)。

**非 openkal 的子进程**(比如 `bash`):在 fd `3+i` 上拿到目录,这是 POSIX 的惯例位置;同时能看到这个环境变量。和现在相比只多了一个变量。

### 4.2 是否符合 openkal 设计规范

| 检查 | 结论 |
|---|---|
| §7.1 自然性 | 子进程启动时读一次,只建一张原本就有的 preopen 表,没有注册表也没有名字解析器。先例:systemd 的 `sd_listen_fds`,用环境变量加从 3 开始的描述符加 pid 绑定,是 POSIX 上传递命名描述符的原生做法 |
| §3.4(不要求解析无界的名字方案) | 只有一种固定的内部格式,不是名字方案;规范里不出现这个变量名 |
| §7.2(handle 不透明) | 变量里写的是实现自己的描述符,属于实现内部 |
| §11 第 18 条 | 传递的是**带名字的 grant**,正是这一条说的"与规范一致的形状" |
| §7.13(新条款) | 子进程接管后马上设 CLOEXEC,grant 不会继续往下传 |
| 和平台无关 | 形状是"环境块加可继承的 handle 加 pid 绑定"。Windows 上被继承的句柄在子进程里**值不变**,`HANDLE_LIST` 可以把 grant 也列进去,同一种编码(`<fd>` 换成 HANDLE 值)可以直接用。本轮 Windows 继续拒绝,将来实现时不需要改规范 |

**不采用的方案**:
- 只靠位置、不传名字:违反 `kal_preopen.name` 和 §11 第 18 条。
- 用 `/proc/self/fd` 加 readlink 反查:那是名字解析器,违反 §7.1。
- 在规范里定义变量名:把一个环境的形状写进了规范,违反 §7.1。

### 4.3 交互细节

- `kal_env_*` 会看到 `KAL_PREOPENS`。**不隐藏**。pid 绑定已经让过期的值失效,下一次 spawn 时也会剔除。隐藏需要在枚举时跳过一个名字,可以以后再做,这一轮不做。
- openkal-musl 程序的 `environ` 里也会有这个变量,不受影响。musl 的 `posix_spawn` 恒传 `grants = 0`,也就是"没有要求",所以继承来的变量会被剔除。
- report pipe 的下限仍然是 `3 + n`;`max_grants = 16` 不变。
- 原型没有处理移开描述符时的 `EMFILE`:失败要经 report pipe 报告,退出码 127。

---

## 5. 规范改动(D4:符合规范,进入 0.14.1)

- **S1 §7.13 What a started program receives**:

  > A program started by `kal_process_spawn` shall receive the three streams and the directories its caller granted, and no other handle — including a handle the calling program itself received from its own environment. Where the environment's own mechanism for starting a program that needs an interpreter requires a handle to survive the start, an implementation may leave one; it shall name no more than the program itself, and shall be left only for a program that needs it.

  §11 第 18 条改为引用 §7.13。

- **S2 `process.h` 中 `grants` 的注释**补两句:
  1. grant 在 POSIX 上不是隔离,隔离是环境的责任(引用 §11 第 6 条);
  2. 一个实现如何把 grant 交给子进程,由实现自己决定,规范只规定子进程通过 `kal_fs_preopen` 读到什么。

- **S3 conformance**:
  - 声明了 `GRANT_DIR` 的实现:conformance 用 `OKC_SELF` 重新执行自己(这个环境变量由 `run-conformance.sh` 设置;没设就记为 not observed)。以 `{ "a" = 目录1, "b" = 目录2 }` 授予,子进程检查名字、标记文件以及 count;再用一组源描述符互相占位的顺序跑一次(覆盖 B/C);再测 `grants` 非空但 `count = 0` 的情况。
  - 没声明的实现:非空 grants 必须返回 `not_supported`(和现有的"a lifetime this implementation cannot bind is refused"一样)。
  - §7.13 本身没法跨平台观察,§9 要求各实现在自己的 additions 里检查。

- **版本**:没有改任何声明,符合 §8,发 **0.14.1**。`version.h` 的 `KAL_VERSION_PATCH` 设为 1,并通过 `tools/check-version.sh`。

---

## 6. 复用贡献者 PR 的规则

1. **贡献者的 commit 一个都不动**:不 rebase、不 amend、不 squash,保留作者、签名(verified)和原始提交信息。
2. **我们的改动作为新 commit 追加**:
   - 作者是本地 git 身份 `SPeak <speakshen@163.com>`(也就是 Sunrisepeak);
   - 尾注 `Co-authored-by: speak-agent <248744407+speak-agent@users.noreply.github.com>`,和 tinyhttps f26a77f 的现有做法一致。
   - 如果你希望主作者是 speak-agent,反过来写即可。
3. **和 base 的冲突用 merge 解决**(`git merge origin/<base>`,在 merge commit 里解决),不 rebase。
4. **用"Create a merge commit"合并 PR**,三个仓库都允许这种方式。merge commit 的提交信息里写明 PR 号、贡献者和维护者追加了什么,并带上 `Co-authored-by` 尾注。
5. **推送**:用 `GH_TOKEN=$(gh auth token --user Sunrisepeak)` 推到 `yspbwx2010/<repo>:<branch>`,不切换全局账号。
6. **在 PR 里留言**:说明追加了哪些 commit、为什么,并感谢贡献者。

---

## 7. 版本与依赖(每个项目只发一次)

### 7.1 发版清单(D6 推荐:补丁版本)

| 项目 | 现在 | 本轮 | 内容 | 需要发版吗 |
|---|---|---|---|---|
| openkal(规范) | 0.14.0 | **0.14.1** | §7.13、§11 第 18 条、`process.h` 注释、conformance S3、`run-conformance.sh` 的 `OKC_SELF` | 是 |
| openkal-linux | 0.15.0 | **0.15.1** | #31(P)+ c2(重试)+ c3(A)+ grants PR(B/C/D) | 是 |
| openkal-macos | 0.12.0 | **0.12.1** | A-mac + B/C/D | 是 |
| openkal-windows | 0.10.1 | 不变 | 只加 §7.13 检查(测试不随包分发) | **否** |
| openkal-musl | 0.19.2 | 不变 | `FDOP_CLOSE` 注释改为引用 §7.13(随下一次正常发版带出) | **否** |
| openkal-llvm-runtime | 0.15.2 | 不变 | — | **否**(依赖解析会拿到 linux 0.15.1) |
| emscripten、uefi、opensbi | — | 不变 | 不提供 spawn | **否** |
| tinyhttps | 0.3.2 | **0.4.0** | #21 + #22 + 维护者 commit;缺 CA 包时失败是行为变化,所以升 minor | 是 |
| mcpp-index | — | 登记 4 个版本 | openkal 0.14.1、linux 0.15.1、macos 0.12.1、tinyhttps 0.4.0 | 是 |

**为什么用补丁版本**:
- 所有改动都是让实现符合**已有**的约定,没有改任何声明。
- mcpp 的约束默认是 caret,所以 `openkal = "0.14.0"`、`openkal-linux = "0.15.0"`、`openkal-macos = "0.12.0"` 都能匹配到补丁版本,下游不用重发,修复就能到达 #30 的报告者(他用的是 llvm-runtime 0.15.2 → musl 0.19.2 → linux)。
- 如果改用 minor 版本(0.16.0 等),musl 和 llvm-runtime 都得跟着各发一次,规范升到 0.15 还会迫使全部九个包为改依赖版本号重发一次(0.14 那一轮就是这样)。

**补丁版本要承担的**:
- 用户能看到的变化:脚本的 `$0` 变成 `/dev/fd/N`,脚本的 comm 显示成解释器名;A 会让"意外拿到某个描述符"的调用者失效。这些必须写在 CHANGELOG 第一行。
- **已核实**:llvm-runtime 是源码包。mcpp-index 的描述里写明它"does NOT carry … a prebuilt binary for any target",在使用者的构建里现场编译,所以不会把旧的 linux 打包进去。**发布后仍要验证**:用 #30 报告者的 `mcpp.toml` 重新解析依赖,确认解析到 openkal-linux 0.15.1。已有 `mcpp.lock` 的用户需要执行一次 update,这一点写进 CHANGELOG 和 GHSA。

### 7.2 依赖图和顺序

```
 tinyhttps(独立,可以最先做)
   #21 + 维护者 c(examples/openkal, README, Windows CI)──merge──┐
   #22 + merge master 解决冲突 + 维护者 c(五条检查清单)──merge──┴→ 0.4.0 + GHSA → index

 openkal 一族
   openkal-linux:  #31 c1(贡献者) + c2 + c3 ──merge→ main(不发版)
                         │ (先合并,grants PR 基于它)
                         ▼
                   grants PR [分支 openkal-0.14.1] ──┐
   openkal-macos:  A-mac + grants PR [openkal-0.14.1] ┤ CI 按同名分支取规范
   openkal-windows: §7.13 检查 PR [openkal-0.14.1] ──┤
   openkal(规范):  S1/S2/S3 PR [openkal-0.14.1] ─────┘ 最后合并、最先发布(沿用 §15.9)
                         ▼
   发布:openkal 0.14.1 → openkal-linux 0.15.1 / openkal-macos 0.12.1 → mcpp-index
                         ▼
   验证:用 #30 的原始程序(spawnprobe + bwrap)经 llvm-runtime 0.15.2 重新解析依赖后复测
   openkal-musl:注释改动合入 main,不发版
```

**硬依赖**:
1. #31 先合并,grants PR 基于它。两者都改子进程分支的开头。
2. #31 的 ELF 测试必须和 c3(A)在同一个 PR 里,否则会时好时坏。
3. 规范 PR **最后合并、最先发布**(沿用以往做法):实现的 CI 通过同名分支 `openkal-0.14.1` 取规范的工作树来验证;发布时规范先发,实现再发。
4. #31 是贡献者的分支(`fix/spawn-base-descriptor`),它的 CI 只能取规范 main。#31 不依赖规范改动(测试都在实现自己的 additions 里),所以没有问题。
5. tinyhttps 必须先合 #21 再处理 #22(语义冲突)。#22 由我们用 merge 解决,不需要等作者。

---

## 8. tinyhttps

**#21,追加一个维护者 commit**:
- `examples/openkal`:`statusText` 以 `certificate verification failed`、`no CA certificate bundle`、`TLS handshake failed` 开头时返回 1,不再当成没网络。
- README 的 openkal 段落:openkal-windows 走 POSIX socket,不读证书库,需要 `SSL_CERT_FILE`;在 openkal 上 TCP 连接超时不生效。
- **D5 是什么意思**:#21 里有一段在 Windows 上读系统证书库(`CertOpenSystemStoreW`)的代码。作者没有 Windows 环境,这段代码**从来没在 Windows 上编译或运行过**,tinyhttps 的 CI 也只有 Ubuntu。问题是要不要合入一段没验证过的代码。选项:
  - (a) 标注"未验证"后合入;
  - (b) 拆出去;
  - **(c,推荐)在这个维护者 commit 里给 tinyhttps 的 CI 加一个 `windows-2022` 任务**(openkal-windows 已经证明 mcpp 在 windows-2022 上用 msvc/llvm 能跑),编译并运行一个读证书库再连接 `https://` 公网地址的测试。用证据代替选择。如果 mbedtls 3.6.1 在 Windows 上用 mcpp 编译不过,退回到 (b),Windows 证书库作为单独的 PR。

**#22,由维护者处理(不等作者)**:
- 在 #21 合并后,把 `origin/master` merge 进贡献者的分支,在 merge commit 里解决冲突;
- 再追加一个 commit,落实五条检查清单:
  1. `fail()` 改为调用 `drop_transport()`;
  2. 移动操作同时搬 `error_` 和 `lower_`;
  3. `open_connection` 合并两处错误来源;
  4. https 代理 TLS 失败时拼上 `tls->error()`;
  5. 手工合并 CHANGELOG。
- 这一条要补测试:代理证书校验失败时的原因能出现在 `statusText` 里。

**发布**:#21 和 #22 都合并后发一次 **0.4.0**,同时发 GitHub Security Advisory(#20,受影响版本 ≤ 0.3.2)。

---

## 9. 全局 review(写完 v2 之后从头核对)

| # | 检查 | 结论或处理 |
|---|---|---|
| 1 | 修复是否都有规范依据,而不是借修复引入新形状 | P、A、B、C、D 都对应 §2 里已经写下的约定;唯一新增的是 §7.13 文本,它只把 §11 第 18 条的事实升为要求 |
| 2 | 是否和平台无关 | 原则和协议形状与平台无关;机制(`close_range`、逐个 `fcntl`、`HANDLE_LIST`)每个平台各不相同。Windows 已经满足 A,并为 grant 留了位置 |
| 3 | 版本号是否和"不改声明"一致 | 规范 0.14.1、实现补丁版本;`KAL_VERSION_PATCH` 用 check-version.sh 检查 |
| 4 | 是否可能连锁重发 | caret 约束可以避免;已核实 llvm-runtime 是源码包,没有预编译产物。剩下的只有"已锁定的用户要 update",写进 CHANGELOG 和 GHSA |
| 5 | 贡献者的签名 commit 是否会被改写 | 不 rebase、不 squash,用 merge commit 合并(§6) |
| 6 | CI 能否看到规范改动 | grants、macos、windows 的 PR 用同名分支 `openkal-0.14.1`;#31 不需要 |
| 7 | 新测试是否稳定 | ELF 测试必须和 A 一起合入;S3 依赖 `OKC_SELF`,没设时记为 not observed,不会误报 |
| 8 | 安全披露 | tinyhttps#20 发 GHSA;**openkal-linux#30 也应该发 GHSA**(对在沙箱里起命令的消费者来说是沙箱逃逸,受影响 0.12.0–0.15.0)。这一条 v1 漏了 |
| 9 | macOS 的判断都来自读代码 | 由 openkal-macos CI 验证;macOS 没有 `close_range`,A-mac 用逐个 `fcntl` |
| 10 | 低于 5.11 的内核和 Android | `close_range` 返回 ENOSYS 时回退到逐个 `fcntl`(D3) |
| 11 | 环境变量协议的边界 | pid 绑定、spawn 时剔除、名字用长度前缀、含 NUL 的名字报错、fd 先 `fstat` 确认是目录、子进程接管后设 CLOEXEC;不隐藏这个变量,已注明理由 |
| 12 | grant 被误当成隔离 | 在 `process.h` 和 README 里写明不是隔离(§3.2) |
| 13 | 发布流程 | 沿用以往约定:打不带 `v` 的 tag、发 GitHub release、用 `gtc` 把同一个 tarball 镜像到 GitCode(两边 sha256 一致)、登记 mcpp-index(注意 `xpm.macosx` 的版本一致性检查) |
| 14 | 收尾 | 在 #20、#30 回复修复所在的版本并关闭;给 mcpp 开并行测试漏 pipe 的 issue |

---

## 10. 决策汇总

| # | 决定 | 状态 |
|---|---|---|
| D1 | grant:现在就实现(§4) | **v2 改为推荐实现**,等你确认 |
| D2 | P 的完善和 A 追加到 #31 | 已同意 |
| D3 | `close_range` 遇到 ENOSYS 时回退到逐个 `fcntl` | 已同意(兼容性优先) |
| D4 | §7.13 进入规范 | 已同意(核对符合规范);以 0.14.1 发布 |
| D5 | tinyhttps 的 Windows 证书库 | 推荐 (c):加 Windows CI 验证;编译不过就拆出去 |
| D6 | 版本策略 | 推荐补丁版本(§7.1),等你确认 |

## 11. 执行顺序

1. tinyhttps:在 #21 上追加维护者 commit(含 Windows CI)→ 合并 → 对 #22 做 merge 并追加 commit → 合并 → 发 0.4.0、GHSA、登记 index。
2. openkal-linux:在 #31 上追加 c2、c3(含 ELF 测试)→ CI → 用 merge commit 合并。
3. 在四个仓库(openkal、linux、macos、windows)都建 `openkal-0.14.1` 分支,各开一个 PR;CI 两两互取。
4. 规范 PR 最后合并;依次发布 openkal 0.14.1 → linux 0.15.1 / macos 0.12.1 → 登记 index。
5. 验证:用 #30 的原始程序重新解析依赖后复测(包括 bwrap);确认解析到的版本。
6. GHSA(openkal-linux),在 #20、#30 回复并关闭,openkal-musl 注释改动合入 main,给 mcpp 开 issue。

---

## 12. v3 自我 review(实现前,多角度)

| 角度 | 发现 | 修订 |
|---|---|---|
| 无感升级 | mcpp 对 0.x 的 caret 取"最左非零位":`^0.3.0` 等价于 `<0.4.0`(`modules/versioning/src/version_req.cppm`)。tinyhttps 若发 0.4.0,写 `tinyhttps = "0.3.0"` 的消费者(包括报问题的 re-cloud-code)**拿不到证书校验修复** | tinyhttps 发 **0.3.3**。缺 CA 包时失败是安全修复带来的行为变化,在 CHANGELOG 第一行说明;代理是增量功能,不影响已有调用 |
| 无感升级 | 新父进程配旧子进程:子进程不认识 `KAL_PREOPENS`,看到的仍是 `cwd` 加 `/`,即现状;旧父进程配新子进程:没有这个变量,新子进程也退回现状 | 混合版本一律退回现状,不会出错。写进 CHANGELOG |
| 一致性 | kit 的 `kal::fs::working()` 和 openkal-musl(`okm_fd.c`)都把 **preopen 0** 当成程序的工作目录;musl 还按 preopen 名字做前缀匹配来解析绝对路径 | 规则写进 `process.h`:**授予目录时,第一个 grant 就是被起程序眼里的工作目录**(和"第一个 preopen 是工作目录"的现有约定一致);名字由调用者决定,要让 C 程序能解析绝对路径,名字应当是绝对路径(如 `/`、`/work`),这和 WASI preopen 的做法一样。kit 的 `working()` 注释同步修改 |
| 稳定性 | 父进程关了 fd 0–2 且没有传对应的流时,子进程里 `openat` 拿到的 exe 描述符可能落在 0–2,成为脚本的 stdin。#31 原样存在这个问题 | exe 描述符若小于 `3+n`,用 `F_DUPFD_CLOEXEC` 移到 `≥3+n` |
| 稳定性 | 移动源描述符时遇到 `EMFILE` | 经 report pipe 报告,退出码 127,和其他启动失败一样 |
| 一致性(CI) | 规范 CI 用**字符串相等**比较规范版本和实现的 `openkal = "…"`;规范升到 0.14.1 后,写着 `"0.14.0"` 的实现在规范 CI 里会失败,包括 Windows | 开发阶段 linux、macos、windows 的依赖写成开发线形式 `{ git, branch = "openkal-0.14.1" }`;规范发布后改为 `"0.14.1"` 再合并。Windows 因此要有一个 PR(依赖版本 + §7.13 检查),但不发版 |
| 一致性(CI) | 规范 CI 克隆 `mcpplibs/openkal-linux` 并检出同名分支,而 #31 的分支在贡献者的 fork 里 | 在 `mcpplibs/openkal-linux` 推一个和 #31 head 相同的镜像分支 `openkal-0.14.1`,并用 `workflow_dispatch` 在它上面跑 openkal-linux 的 CI,和规范分支配对 |
| 优雅简洁 | 协议需要定宽 pid 字段 | 父进程在 fork 之前构建环境块,子进程用 `getpid` 写 10 位数字,不需要额外的变量 |
| 跨平台 | macOS 的 fork 也是写时复制,子进程可以安全修改环境块 | 和 Linux 共用同一种格式,只是各自实现一份 |
| 测试覆盖 | 原计划的 S3 需要 conformance 重新执行自己 | `run-conformance.sh` 导出 `OKC_SELF`(相对于工作目录的可执行文件路径);没有时记为 not observed |

### 12.1 协议的精确定义

```
KAL_PREOPENS=<pid>{;<fd>,<len>,<name>}
  pid   10 位十进制,左侧补零,由子进程在 fork 之后写入
  fd    十进制,子进程中该目录所在的描述符(POSIX 上为 3+i)
  len   十进制,name 的字节数
  name  len 个字节,不含 NUL;可以含 ';' 和 ','
```

例:`KAL_PREOPENS=0000012345;3,1,/;4,5,/work`。`grants` 非空但 `count = 0` 时,值只有 pid。

子进程端:只认 pid 等于自身的值;每个 fd 用 `fstat` 确认是目录,不是就把这一项的 handle 置 0;对每个 fd 设 CLOEXEC;名字直接指向环境块(环境块在进程的整个生命周期里都有效)。

父进程端:调用者 envp 里所有以 `KAL_PREOPENS=` 开头的项都剔除;只有 `grants != nullptr` 时才追加新的。

## 13. 执行计划(任务与依赖)

| 流 | 仓库 | 内容 | 前置 | 执行者 |
|---|---|---|---|---|
| T1 | tinyhttps | #21:维护者 commit(examples/openkal、README、Windows CI);#22:merge master,加五条检查清单和测试;版本 0.3.3 | 无 | 并行 |
| L1 | openkal-linux(#31 + 镜像分支) | P'、A、B/C/D、exe 描述符下移、协议、测试、0.15.1、CHANGELOG/README | 无 | 主线 |
| S1 | openkal | §7.13、§11 第 18 条、`process.h`/`space.h`/kit 注释、conformance S3、`run-conformance.sh`、0.14.1 | 协议定稿(§12.1) | 主线 |
| M1 | openkal-macos | 移植 L1(A 用逐个 `fcntl`;没有 P) | L1 的协议代码 | 并行 |
| W1 | openkal-windows | §7.13 检查、依赖改为开发线 | S1 分支存在 | 并行 |
| U1 | openkal-musl | `FDOP_CLOSE` 注释引用 §7.13(不发版) | S1 | 主线 |
| R | 发布 | 规范 0.14.1 → linux 0.15.1 / macos 0.12.1 → tinyhttps 0.3.3 → mcpp-index | 全部 CI 通过 | 主线 |
| V | 验证 | xlings subos 沙箱(CN 镜像)只写版本号解析;re-cloud-code 真实场景(bwrap) | R | 主线 |
