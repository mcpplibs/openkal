# 被起的程序收到什么:实施记录

日期:2026-10-01。依据:设计 `2026-10-01-what-a-started-program-receives-design.md`(v3),验证脚本 `2026-10-01-what-a-started-program-receives-verify.sh`。

## 1. 结论

- openkal-linux#30(0.12.0–0.15.0 每个被起的程序都继承基目录描述符,沙箱内可借此访问宿主文件系统)与 tinyhttps#20(`verifySsl = true` 时不校验证书)均已修复并发布。
- 规范新增条款 7.13:被起的程序只收到三个流与被授予的目录;被授予的目录以 preopen 的形式、按顺序与名字到达。conformance 套件从效果上观察 grant。
- 修复过程中发现并一并修正的同源缺陷:调用者自身继承的描述符被转交(A);已在目标号上的 grant 丢失(B);顺序放置覆盖其他 grant 与 base(C);子进程从不读取 grant,而 Linux 与 macOS 均声明支持(D)。
- 全部发布为补丁版本。mcpp 中不带运算符的版本号是精确锁定(设计 §14),因此使用者要改一行版本号才能拿到修复;锁定规范的实现、openkal-musl 与 openkal-llvm-runtime 随之各发布一次,使含 openkal 0.14.1 的图可以解析。

## 2. 发布

| 包 | 版本 | PR | 合并提交 | sha256(GLOBAL 与 CN 一致) |
|---|---|---|---|---|
| openkal | 0.14.1 | mcpplibs/openkal#43 | `c478374` | `3204cf5265c7c8551a5000cb675520c64e0f4c01855b190528b953904418e02e` |
| openkal-linux | 0.15.1 | mcpplibs/openkal-linux#31(贡献者 PR,追加维护者提交,merge commit 合并) | `3493ed4` | `b7ca4e17c4b93af1b382f0a657af9f2515a873ef276b802ea8e3d192d599fc4a` |
| openkal-macos | 0.12.1 | mcpplibs/openkal-macos#24 | `3098999` | `49f43552dbdcc45a33fe7b0c93a4651e9ea29f28f1194963e3c6932a115409a5` |
| tinyhttps | 0.3.3 | mcpplibs/tinyhttps#21、#22(贡献者 PR,追加维护者提交,merge commit 合并) | `f461940` | `18ea1c333d72e02a5fd5c1486612838cc8fa102db8b3893958c8ea7b249c8a18` |
| openkal-windows | 0.10.2 | mcpplibs/openkal-windows#28(§7.13 检查)、#29(版本) | `5489b8a` | `3e05e62340c486e742a5ffc4cd5f460732e8b6c46a7151f5aa8971512a59a722` |
| openkal-opensbi | 0.8.1 | mcpplibs/openkal-opensbi#18(只改锁定版本) | `d168407` | `4a08467194a82308683950b6ca15733563baaae52b733749f5c65898e9c066ad` |
| openkal-uefi | 0.8.1 | mcpplibs/openkal-uefi#15(只改锁定版本) | `9e3d3e2` | `8f78a0cda718f79fe23a6b22a16d645b5ac22e071e2d34318b611333f5a7ad52` |
| openkal-emscripten | 0.3.1 | mcpplibs/openkal-emscripten#4(只改锁定版本) | `ff3b56e` | `503e7af9ad7a2b70222eff3f0d198bf61f8efb4ee27b26a147df21d293e6ad3a` |
| openkal-musl | 0.19.3 | mcpplibs/openkal-musl#44(注释引用 §7.13)、#45(锁定版本) | `c109b4d` | `4d772a741128c070b0ec5d1e8357d5324c0d4085340feea8468a0b285722b1d6` |
| openkal-llvm-runtime | 0.15.3 | mcpplibs/openkal-llvm-runtime#32(锁定 musl 0.19.3) | `ce0a550` | `a0d17ad7a343d6bcdbb1ad58b77864df7ad570edaaad74a1352aa2b10311f300` |
| mcpp-index | — | mcpplibs/mcpp-index#498、#499、#500、#501(按依赖分四批) | `98e7c26`、`3e14517`、`e15a35c`、`c0c5e01` | — |

贡献者 Cloud_Yun 的五个签名提交(openkal-linux `5104f8d`;tinyhttps `1d14c23`、`08d8540`、`edff1ed`、`0733e5d`)未经改写,合并后仍为 verified。维护者提交带 `Co-authored-by: speak-agent`。

## 3. 各仓库的改动

**openkal(规范,0.14.1)**:条款 7.13;§11 第 18 条改为引用 7.13;`process.h` 写明 grant 的顺序、名字、首个 grant 即工作目录、grant 不构成隔离、传递方式由实现决定;`fs.cppm` 中 `working()` 的注释同步;conformance 新增 `report_grants`、`report_no_preopens` 两个 errand,声明 `GRANT_DIR` 的实现观察效果,未声明的实现观察拒绝。对 openkal-linux 0.15.0 这两项观察均失败,对 0.15.1 均成立。

**openkal-linux(0.15.1)**:
- P:程序经由指向自身文件的描述符启动,以 `O_CLOEXEC` 打开;内核对需要解释器的程序在不可回退点之前返回 `ENOENT`,此时清除标志后重试。普通可执行文件不留任何描述符,脚本只留一个指向自身的描述符。该描述符若落在 0 到 `3+n` 之间,则移到其上。
- A:放置完成后对 `3+n` 以上全部描述符设置 close-on-exec(`close_range`,内核低于 5.11 时依次退回 `/proc/self/fd` 与描述符上限)。
- B、C:放置前先把全部源(grant、流、base、work)移到 `3+n` 以上。
- D:`KAL_PREOPENS=<pid>{;<fd>,<len>,<name>}`,由子进程写入自身 pid;`kal_fs_preopen` 在 pid 匹配时只枚举 grant。
- 测试 `tests/conformance_spawn.cpp`:对 0.15.0 失败 7 项,对 0.15.1 全部通过。

**openkal-macos(0.12.1)**:A(枚举 `/dev/fd`,退回描述符上限,上限取 `OPEN_MAX`)、B、C、D,与 Linux 采用同一协议;P 不存在(以绝对名启动)。

**openkal-windows**:不改行为。新增测试观察:调用者自行设为可继承、但未放置的句柄不会到达被起的程序;非空 grants 被拒绝。

**tinyhttps(0.3.3)**:`VERIFY_REQUIRED`,失败原因写入 `statusText`,Windows 读取系统 ROOT 存储(新增 windows-2022 CI 任务实测);代理认证、https 代理、SOCKS5;合并时落实五条检查清单,并新增"https 代理证书未通过校验时报告原因且不发送 CONNECT"的测试。

## 4. CI

| 仓库 | 运行 | 结论 |
|---|---|---|
| openkal #43 | 36859540216(第 3 次尝试) | 12/12 通过;Linux 与 macOS conformance 192 held、0 did not hold;Windows 145 held、0 did not hold |
| openkal main(合并后,实现合并后重跑) | 36860483476 | 通过 |
| openkal-linux #31 | 36860588323(PR)、36860663933(镜像分支,与规范同名分支配对) | 通过 |
| openkal-macos #24 | 36860610573 | 通过 |
| openkal-windows #28 | 36860629222 | 通过(三个原生 Windows 任务加 wine 交叉构建) |
| openkal-musl #44 | 36859755231 | 通过 |
| tinyhttps #21、#22、master | 36859176687、36859638490、36859894503 | 通过 |
| openkal-windows #29、opensbi #18、uefi #15、emscripten #4 | 各 PR 的全部任务 | 通过 |
| openkal-musl #45 | 5 个任务;解析到 openkal-linux 0.15.1 | 通过 |
| openkal-llvm-runtime #32 | 5 个任务(含在 macOS 与 Windows 上运行 Linux 构建的产物),`-- failures: 0 --` | 通过 |
| mcpp-index #498、#499、#500、#501 | 含 openkal 兼容性测量(linux 与 wine 下的 windows) | 通过 |

## 5. 生态验证

### 5.1 xlings subos 沙箱(只从已发布的 index 解析)

`xlings subos new okl0141`;沙箱内 mcpp 2026.10.1.2,xlings 与 mcpp 镜像均为 CN。脚本 `2026-10-01-what-a-started-program-receives-verify.sh`,清单为报告者的原清单改一行版本号。

```
== A. identity and mirror ==
ok: mcpp 2026.10.1.2 from /home/speak/.xlings/data/xpkgs/xim-x-mcpp/2026.10.1.2/bin/mcpp
ok: xlings mirror is CN
== B. openkal 0.14.1 and openkal-linux 0.15.1 resolve, and a granted copy enumerates its grants ==
ok: header 0.14.1 implementation 0.14.1
deferred: the granted copy is observed in host mode
ok: the lock records openkal-linux 0.15.1
== C. a program started through posix_spawn above openkal-musl receives nothing it was not given ==
ok: the reporter's manifest resolves openkal-linux 0.15.1
ok: the reporter's program is built from the published index; its behaviour is observed outside (host mode)
== D. tinyhttps 0.3.3 verifies certificates ==
ok: tinyhttps 0.3.3 resolves
ok: [good] SUCCESS 200
ok: [self] FAILED certificate verification failed: The certificate is not correctly signed by the trusted CA
0 assertion(s) failed
```

host 模式(运行沙箱从 index 构建出的静态程序;B 由宿主 mcpp 从同一 index 构建):

```
== B. a copy started with grants enumerates exactly them (host, from the published index) ==
ok: a granted copy enumerates: 2 [/work] [/]
== C. a program started through posix_spawn above openkal-musl receives nothing it was not given (host) ==
ok: an ordinary executable inherits no directory descriptor
ok: a script starts, holds no directory, and reads its name as /dev/fd/9
ok: a name that is not there is still ENOENT
0 assertion(s) failed
```

沙箱由 proot 实现,行为观察分到 host 模式的原因已在脚本中写明,也是验证过程中发现的三件事:

1. proot 不拦截 `execveat`,由它启动的程序的解释器按宿主路径解析;openkal-linux 任何版本在沙箱内启动动态链接程序都得到 `ENOENT`。
2. 沙箱的根在宿主上是空目录,绑定挂载由 proot 模拟;相对根描述符解析的名字在宿主树中查找,`/bin/sh` 等因此不存在。
3. 多线程 `ld.lld` 在 proot 下一次以 `malloc(): unaligned tcache chunk detected` 中止、一次停止推进;脚本以 `-Wl,--threads=1` 链接。以命令行传入 16 KB 的脚本时伴随 `ptrace(PEEKDATA): Bad address` 与无关文件操作失败;脚本改为写入沙箱的 `/tmp`。

上述三点均为沙箱环境的性质,与本轮改动无关;同一检查在宿主上得到预期结果。

### 5.2 报问题的真实项目 re-cloud-code

`cloud-teahouse/re-cloud-code` 更新到 `0b44f63`,用项目自己的 mcpp 家(引擎 2026.10.1.2,CN 镜像)与它规定的资源上限(`systemd-run --scope -p CPUQuota=300% -p MemoryHigh=5G`,`MCPP_JOBS=2`)跑全工作区测试,目标为默认的 `x86_64-linux-musl`。改动只有清单里的版本号:16 个清单中 `openkal-llvm-runtime = "0.15.2"` 改为 `"0.15.3"`,`tinyhttps = "0.3.0"` 改为 `"0.3.3"`(在 scratchpad 的 worktree 中进行,未提交到该仓库)。

| 项 | 改前(已发布的旧栈) | 改后 |
|---|---|---|
| 解析结果 | openkal-linux 0.15.0、openkal-musl 0.19.2、tinyhttps 0.3.0 | openkal-linux 0.15.1、openkal-musl 0.19.3、openkal-llvm-runtime 0.15.3、tinyhttps 0.3.3 |
| 全工作区 | `1/16 member(s) failed; 150 passed; 1 failed` | `workspace result ok. 16 member(s); 151 passed; 0 failed` |
| `test_process_stream`:被起程序除 0/1/2 外继承的宿主描述符 | run_shell_stream 1 个、detach_session 1 个(列表中为 `9 -> /`) | 0 个、0 个 |
| `test_llm_transport`:TLS 校验开启,证书只含 IP SAN `127.0.0.1`,`SSL_CERT_FILE` 指向它 | 未运行到(见下) | ok |

改前的一项失败是 `test_llm_transport` 中 `openssl req` 失败:PATH 上优先的 xlings openssl 3.1.5 写死了不存在的配置路径。改后的运行设置了 `OPENSSL_CONF=/usr/lib/ssl/openssl.cnf`,使该测试真正执行;它在 `verifySsl = true` 下通过,说明 tinyhttps 0.3.3 按 IP SAN 校验 IP 字面量主机(另以 SAN 为 `127.0.0.2` 的证书确认会被拒绝)。该项目的测试在源码注释中预期了这一点:"上游校验生效后测试不用改:信任根指向假端点自己的证书"。

`test_process_stream` 的严格判据在该项目中以 `#if defined(__GLIBC__)` 限定,在 openkal 路径上只打印计数并 SKIP,注释的理由是"能力型内核够不着继承来的宿主 fd"。自 openkal-linux 0.15.1 起计数为 0,该判据可以在 openkal 路径上同样启用;是否启用由该项目决定。

## 6. 发现但不在本轮范围内的问题

- **mcpp 2026.9.30.2 并行测试把兄弟测试的 pipe 泄漏进测试进程。** 在该版本上可稳定复现,在 2026.10.1.2 上不再出现(2026.10.1.1 #752 改写了输出流),因此不再报告。openkal-linux 的 A 修复同时保证了即使测试进程继承了这类描述符,也不会继续传给它起的程序。
- **xlings 提供的 openssl 3.1.5 写死了不存在的配置路径**(`/home/xlings/.xlings_data/xim/xpkgs/fromsource-x-openssl/3.1.5/ssl/openssl.cnf`),PATH 优先命中它时 `openssl req` 失败。re-cloud-code 的 `test_llm_transport` 在本机因此失败,与本轮改动无关;设置 `OPENSSL_CONF` 可以绕开。
- **compat.mbedtls 在 msvc 下以 `-lbcrypt` 声明链接库。** tinyhttps 的 CI 使用 mcpp 2026.8.29.1 时链接失败,已在 tinyhttps 中以 `#pragma comment(lib, "bcrypt.lib")` 解决;较新的 mcpp 是否转换 `-l`,待核实。
- **本机 AppArmor 限制非特权 user namespace**(`kernel.apparmor_restrict_unprivileged_userns = 1`),bwrap 与 `unshare -Urm` 均不可用。因此 #30 的沙箱逃逸在本机以其机制(被起程序持有的描述符)验证,而非以 bwrap 复现。

## 7. 实施后的生态级自我 review

| 角度 | 结论 |
|---|---|
| 架构与规范一致性 | 每处修复都对应已写下的约定(§7.13 由 §11 第 18 条升格而来,grant 的语义来自 `process.h`);唯一新增的规范文本是 §7.13。grant 的传递方式不进入规范,两个 POSIX 实现共用一种格式。 |
| 跨平台 | 原则与平台无关;机制分别是 Linux 的 `close_range`/`execveat`、macOS 的 `/dev/fd` 枚举、Windows 的 `HANDLE_LIST`。Windows 不声明 `GRANT_DIR` 并被观察到拒绝 grant;协议格式为它日后实现 grant 留有位置(句柄值同样可列在变量中)。 |
| 兼容性与升级 | 声明未变,规范与各实现均为补丁版本。mcpp 中不带运算符的版本号是精确锁定,因此修复经一次清单改动到达使用者(re-cloud-code 为 20 行同类改动);生态内锁定规范的包各发布一次,保证含 0.14.1 的图可以解析。用户可见的行为变化(脚本的 `$0`、缺 CA 包时失败)写在发布说明首段。 |
| 稳定性 | 放置前移开全部源;exe 描述符不落在 0–2;`close_range` 不可用时两级回退;移动失败经 report pipe 报告。grant 的变量以 pid 绑定,调用者传入的同名变量一律剔除。 |
| 测试覆盖 | 规范 conformance 从效果上观察 grant(对旧实现失败、对新实现成立);openkal-linux 与 openkal-macos 各有观察 P、A、B、C、D 的测试,对修复前的源码失败 7 项;Windows 观察 A 与拒绝;tinyhttps 观察证书校验、代理证书错误;端到端以报告者的程序与真实项目验证。 |
| 文档 | 设计 v4 记录了被推翻的前提(§14);发布说明中关于升级的错误表述已更正;各 README 的安装行已指向本轮版本。 |
| 未完成与后续 | 见 §6。GitHub Security Advisory 以草稿形式建立,待维护者发布(§8)。 |

## 8. 安全通告

两份 GitHub Security Advisory 已建立为草稿,未发布;发布由维护者决定。

- openkal-linux `GHSA-3cvp-337g-p4mw`:受影响 `>= 0.12.0, < 0.15.1`,修复 0.15.1;上层经 openkal-musl 0.19.3、openkal-llvm-runtime 0.15.3 获得。
- tinyhttps `GHSA-cpc8-3w7f-j3qm`:受影响 `<= 0.3.2`,修复 0.3.3。

两者都注明 mcpp 的版本号是精确锁定,使用者须在清单中写出修复版本。
