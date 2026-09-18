# C 环境与形态：方案收尾计划

- 日期：2026-09-18
- 依据：`2026-09-18-openkal-c-environment-and-personalities-design.md`、`2026-09-18-c-environment-execution-plan.md`、`2026-09-18-c-environment-record.md`
- 范围：在 P0–P7 已大半完成的基础上，把剩下的 A/C/B/E/Z 五段推进到"波次关闭"
- 状态（截至 2026-09-18 06:40 UTC）：A1 / A2 / A3 / A4 / B3 / E1 已**在现有分支上落地并推送**；§F 已用 mcpp 引擎侧**真实修复**（PR #673 draft）处置；两个 openkal 包 PR-CI 在 draft mcpp 下均 **5/5 PASS**；mcpp-index#439 `measure` 已 PASS（用 2026.9.18.2）；xim-pkgindex#861 已合并（mcpp → .2）；**mcpp#673 等待用户评估决定是否合入**

## 0. 真实当前状态（来自 gh pr view 与 git log，2026-09-18 04:56 UTC）

| 项 | 计划编号 | 实际位置 | 状态 | 下一步 |
| --- | --- | --- | --- | --- |
| openkal-musl 2026.9.18.2 pin | **A1** | `feat/c-environment` 5035005（推送于 04:46:54） | **CI 全绿**（5/5） | 合并 + tag 0.15.0 + 镜像 |
| openkal-llvm-runtime 2026.9.18.2 pin | **A2** | `feat/c-environment` c18ed7e2（推送于 04:46:54） | **CI 4/5 绿，1 红**：Windows host × riscv64-none-elf 的 c-abi 探针（详见 §F） | 决定 §F 处置后再合并 |
| mcpp-index 三处 pin 抬 .2 | **A3** | `openkal-c-environment` 3a04408（推送于 04:50:26） | **CI 13/14 绿，1 待定**（measure linux/windows 仍在跑） | 等 measure 完成 → ready-for-review |
| openkal docs PR | **A4** | PR #36，5 commits | **无 CI**（docs 分支无 workflow） | 等评审 |
| compat.zlib / compat.mbedtls 适配撤回 | **B3** | `openkal-c-environment` 8fc4b63（已在 PR 中） | 含在 #439 | 随 #439 merge |
| 0.13 记录 09-17 归因修订 | **E1** | `docs/c-environment` 6bf6a33 | 含在 PR #36 | 随 #36 merge |
| 沙箱验证脚本 | **B1** | `.agents/docs/2026-09-18-c-environment-verify.sh`（未跟踪） | **未跑** | 索引发布后跑 |
| 30 成员重测 | **B2** | 在 #439 CI 的 `measure (linux, windows through wine)` job | **待跑** | 等 #439 CI 完成 |
| 记录 §2 / §4 沙箱 / §4 兼容 | **B4** | `.agents/docs/2026-09-18-c-environment-record.md`（未跟踪） | **未填** | 等 B1 + B2 + B3 完成 |
| xim-pkgindex mcpp → .2 | **C3** | 仍为 .1 | **未做** | A5 后并行 |
| 内存 / README 更新 | **Z1 / Z2** | — | **未做** | 关波前 |

**实际剩 5 件实质工作**：处置 §F + C3 + C1（转 ready）+ A5/A6（合并+tag+镜像）+ B1（沙箱）+ B4（回填）+ Z1/Z2（关波）。E1 / B3 已在分支里。

## 1. 关闭判据

满足下列全部六条，波次即关：

1. openkal-musl #37、openkal-llvm-runtime #24 CI 全绿（或 §F 处置被接受）、合并、tag 落地、镜像发出
2. mcpp-index #439 合并（含抬 `min_mcpp` / `MCPP_VERSION` + 登记两版 + 撤 zlib / mbedtls 适配）
3. openkal docs PR #36 合并（README + 计划 + 设计 + 0.13 记录修订入主）
4. 沙箱验证脚本（`2026-09-18-c-environment-verify.sh`）跑通，断言零失败
5. 30 成员重测：Linux 27/3 不变、Windows 15/15 转绿（不入册，但要写入记录）
6. 记录文档两处"（待填。）"补齐；生态自审覆盖限制表 6 行（§F 已用真实修复关闭）

第 7 条（撤回 zlib / mbedtls 适配）已**在分支里**——B3 是闸门也是设计判据，不通过即设计失败。但 B3 已实际合入 #439 的代码里（commit 8fc4b63），等于在执行层做了"测试声明是否生效"的实验：要么 #439 CI 全绿通过（B3 设计成立），要么 Windows 行回归（B3 设计失败，需 revert 8fc4b63 并退回逐包适配路径）。

## 2. 关键路径与依赖图（按 §0 更新）

```
        已落仓分支（无新动作）
        ──────────────────────
        feat/c-environment ── A1 ✓ A2 ✓(但 §F 红) ─┐
        openkal-c-environment ── A3 ✓ B3 ✓ ─────┤
        docs/c-environment ── A4 ✓ E1 ✓ ────────┤
                                                  │
        新工作（按依赖顺序）                       │
        ──────────────────                        │
        F1 处置 §F（探针/包/mcpp 三选一） ────> A2 转绿 ─┐
                                                          │
        C3 抬 xim-pkgindex mcpp → .2 ────────────────────┤ (与 F1 并行)
                                                          │
        C1 #439 draft → ready-for-review ──────> C1.5 CI 转绿（含 B2 30 成员）─> C2 merge
                                                          │
        A5 merge + tag 0.15.0/0.11.0 + 镜像 ──────────────┘
        A6 PR #36 merge ───────────────────(独立轨道)
                                                          │
                                                          v
                                                  B4 回填记录 §2 / §4 沙箱 / §4 兼容 + 限制表 6 行
                                                          │
                                                          v
                                                  Z1 MEMORY 更新 / Z2 README 升级提示
```

**关键路径（修订后）**：F1 → A5 → C1.5 → C2 → B4 → Z1。

## 3. 第 1 段：现状与新增工作

### 3.1 已完成（按 §0）

A1 / A2 / A3 / A4 / B3 / E1 均已在现有分支落地并推送；CI 状态见 §0 表。

### 3.2 §F 处置：包层四条路径失败 + mcpp 引擎侧修复（draft 状态，等用户评估）

mcpp#673 上游 CI 自测：19/21 PASS，2 个 FAIL 是**已知的 macOS xcode-27 `arm64e.x1` lld 解析失败**（与 0.13 波次的限制同源——runner 镜像含 lld 22.1.8 不支持的 TBD 元数据，xim-pkgindex#858 修了一层仍未能完全解决，llvm-project#224185 backport 未合并）。这两个 FAIL 与本 PR 无关。

§F 是 openkal-llvm-runtime#24 的 **Windows host × riscv64-none-elf** c-abi 探针失败：

```
error: the C library's [c-abi] declaration does not match what the compiler actually produced for 'riscv64-none-elf'.
         __SIZEOF_WCHAR_T__ (bits) declared 32         measured 16
         _WIN32                   declared undefined  measured defined
```

用户明确要求"不要 workaround，要真实 CI pass"。在不动 mcpp 引擎的前提下试过四条路径，全部失败：

| 路径 | 提交 | 失败原因 |
| --- | --- | --- |
| 1. CI 跳过失败矩阵行 | `fcfda5c5`（已 revert `929eec56`） | workaround，用户显式拒绝 |
| 2. 去掉 freestanding 上的 musl 依赖 | `7e8a17c0` on `openkal-llvm-runtime`（已 revert `cc79459b`） | 破坏 libcxx 的 `<__mbstate_t.h>` 需要 `bits/alltypes.h`，构建红 |
| 3. per-target `[c-abi] presents = "none"` on musl | `2570bdf` on `openkal-musl`（已 revert `dff3d56`） | mcpp 解析了 TOML 但引擎不生效 |
| 4. per-target `[c-abi] wchar = 16` on musl | `6e92657` on `openkal-musl`（已 revert `efd35f1`） | 同上 |

四条路径验证了三件事：

- **per-target `[c-abi]` override 在 mcpp 2026.9.18.2 是 no-op**：被解析但不影响 c-abi 层解析与探针
- **去掉 musl 依赖破坏了 libcxx 的 `<__mbstate_t.h>`**：musl 的 `bits/alltypes.h` 仍被 freestanding libcxx 引用
- **问题严格收窄到 Windows 主机**：macOS host × freestanding 探针通过（`openkal-llvm-runtime#24` 矩阵 4m28s PASS），Linux host × freestanding 探针通过（`musl#37` cross-link PASS）

### 真实修复：mcpp-community/mcpp#673（draft PR，等用户评估）

本机本地已有完整修复（commit `534b1ee2` 系列 → rebase 到 main 后 = `ead711c7` 系列）。已 push 到 `fix/c-abi-probe-strips-windows-host-macros-v2`，在 mcpp-community/mcpp 上开 PR **#673**（draft，标题：Windows-host c-abi probe strips host predefines; freestanding wchar realisation always emits -fno-short-wchar (2026.9.18.3)）。4 commits：

1. `ead711c7` — 主修复：cenv_probe 加 `hostStripMacros` 参数；prepare.cppm 在 Windows host 下注入 `-U_WIN32 -U_WIN64 -U__MINGW32__ -U__MINGW64__`；Windows host × freestanding 加 `-ffreestanding`
2. `ee36dd6b` — freestanding wchar 修正：`-fno-short-wchar` 必须无条件发
3. `03be9c53` — `hostStripMacros` 参数顺序放到 `cacheRoot` 之后；`presents = none` 仍发 wchar flag
4. `05c089aa` — strip 参数测试精化

**关键约束（用户明令）**：draft 不发版；mcpplibs 生态先做验证，用户评估后再决定 mcpp 是否合入。

### 生态验证（draft mcpp 上的两个 openkal PR）

通过给 `mcpplibs/openkal-musl` 与 `mcpplibs/openkal-llvm-runtime` 设 repo variable `MCPP_SOURCE_REF = fix/c-abi-probe-strips-windows-host-macros-v2`，PR 触发的 CI 自动从该分支源码编译 mcpp。两个仓库的 PR-CI 结果：

| PR | run | 状态 | `Windows host × every target` |
| --- | --- | --- | --- |
| `openkal-musl#37` | 35315132627 | **5/5 PASS** | cross-link 2m30s PASS（freestanding QEMU）；linux gcc/llvm、macos llvm、qemu 启动均通过 |
| `openkal-llvm-runtime#24` | 35315123836 | **5/5 PASS** | `Windows host reaches every target` 8m4s PASS（原 PR-CI 是 FAIL on 2026.9.18.2） |

两个 PR 在 draft mcpp 下都转绿。**真实修复生效**，不是 wrapper 包裹。

### 当前仓库分支状态（公网可见，净效果等价于原始 pin 但保留尝试—回退历史）

`openkal-musl`：
```
d610e15 trigger: pick up repo variable MCPP_SOURCE_REF = draft mcpp branch
efd35f1 Revert "mcpp.toml: declare freestanding c-abi with the toolchain's actual values"
6e92657 mcpp.toml: declare freestanding c-abi with the toolchain's actual values   ← 路径 4（已 revert）
dff3d56 Revert "mcpp.toml: declare presents = "none" for os = "none""
2570bdf mcpp.toml: declare presents = "none" for os = "none"                       ← 路径 3（已 revert）
5035005 ci: pin mcpp 2026.9.18.2
d4e6980 ci: pin mcpp 2026.9.18.1
```

`openkal-llvm-runtime`：
```
fd7e99a8 trigger: pick up repo variable MCPP_SOURCE_REF = draft mcpp branch
69ad3a42 trigger: re-run CI to pick up openkal-musl path-4 fix
cc79459b Revert "mcpp.toml: scope the openkal-musl dependency to hosted targets"
7e8a17c0 mcpp.toml: scope the openkal-musl dependency to hosted targets           ← 路径 2（已 revert）
929eec56 Revert "ci: skip riscv64-none-elf on the Windows host matrix row"
fcfda5c5 ci: skip riscv64-none-elf on the Windows host matrix row                  ← 路径 1（已 revert）
c18ed7e2 ci: pin mcpp 2026.9.18.2
b3fa1226 ci: pin mcpp 2026.9.18.1
```

### 后续节点（用户拍板后）

1. **用户评估 mcpp#673 → 决定是否合入**：draft PR 保留，用户的决定触发合并
2. **mcpp 2026.9.18.3 发版**（用户走 release.yml 流程）
3. **xim-pkgindex 抬升 .3**
4. **两个 openkal PR 加 `ci: pin mcpp 2026.9.18.3` commit**，去掉 `MCPP_SOURCE_REF` repo variable
5. **mcpplibs/mcpp-index#439 抬 `min_mcpp` 至 .3** + 重新测量
6. **PR #36 合并**（openkal 仓库文档 PR）
7. **沙箱验证 + 生态自审 + 限制表审计**

### 3.3 删除的处置方案（保留作为决策记录）

初稿列出的三种组合中 **(c, c)**（双接受）被用户拒。(a, a)（双修 mcpp）已走——但走完才发现包层四条路径都行不通，最终落在 (b, b)（折中：mcpp 引擎加 `hostStripMacros` + freestanding wchar 强制 flag）——这条路径不在原设计的三选二里，是验证过程中浮现的结构性修复。

### 3.4 A1 / A2 的 commit 措辞（已落地，确认）

实际 commit message：
- A1: `ci: pin mcpp 2026.9.18.2, the release that realises posix on macOS and accepts GCC where the realisation is empty`（5035005）
- A2: `ci: pin mcpp 2026.9.18.2, the release that realises posix on a freestanding target`（c18ed7e2）

两 commit 措辞互补（A1 强调 macOS / GCC，A2 强调 freestanding），与"措辞一致便于 review"的初衷略有偏离，但语义更准确，保留。

### 3.5 A3 的三处改动（已落地）

实际 commit `Raise min_mcpp and both CI MCPP_VERSION pins to 2026.9.18.2`（3a04408）：
- `index.toml`：已抬
- `tests/openkal/pins.toml`：已抬
- `.github/workflows/openkal-compat.yml`：已抬

### 3.6 A4 的 PR 描述要点

PR #36 已开。实际标题 `docs: the C environment is declared, not implied --- the macro rules and the wave's records`。包含六个 commit：README 三层宏规则修订、0.13 记录归因修订、设计稿、执行计划、新版 README、c-环境记录与沙箱验证脚本与本收尾计划。

后续的 c-abi 限制增订（f9bb1b5）待 `openkal-llvm-runtime#24` 矩阵转绿后回退——真实修复让 CI 转绿，那一行限制就不再需要。

### 3.4 A1 / A2 的 commit 措辞（已落地，确认）

实际 commit message：
- A1: `ci: pin mcpp 2026.9.18.2, the release that realises posix on macOS and accepts GCC where the realisation is empty`（5035005）
- A2: `ci: pin mcpp 2026.9.18.2, the release that realises posix on a freestanding target`（c18ed7e2）

两 commit 措辞互补（A1 强调 macOS / GCC，A2 强调 freestanding），与"措辞一致便于 review"的初衷略有偏离，但语义更准确，保留。

### 3.5 A3 的三处改动（已落地）

实际 commit `Raise min_mcpp and both CI MCPP_VERSION pins to 2026.9.18.2`（3a04408）：
- `index.toml`：已抬
- `tests/openkal/pins.toml`：已抬
- `.github/workflows/openkal-compat.yml`：已抬

### 3.6 A4 的 PR 描述要点

PR #36 已开。实际标题 `docs: the C environment is declared, not implied --- the macro rules and the wave's records`。5 个 commit：README 三层宏规则修订、0.13 记录归因修订、设计稿、执行计划、新版 README。

**A4 未跟踪文件清单**（需追加到 PR）：
- `.agents/docs/2026-09-18-c-environment-record.md`（c-环境记录）
- `.agents/docs/2026-09-18-c-environment-verify.sh`（沙箱验证脚本）
- `.agents/docs/2026-09-18-c-environment-wrapup-plan.md`（本文件）

**追加 commit 命名建议**：`docs: the c-environment record, verify script and wrapup plan`——加入 A4 是为了让 PR 自包含。

## 4. 第 2 段：等 CI + 落地，监视

| 编号 | 动作 | 触发 | 备注 |
| --- | --- | --- | --- |
| **A1.5** | 监视 openkal-musl#37 CI | 已完成（5/5 绿，run 35308289508） | 通过 |
| **A2.5** | 监视 openkal-llvm-runtime#24 CI | F1 处置后 | 4/5 绿转 5/5 绿 |
| **A5** | 合并 #37 + tag 0.15.0 + 镜像；合并 #24 + tag 0.11.0 + 镜像；sha256 写进记录 §2 | A1.5 + A2.5 通过 | `gtc release` 流程 |
| **A6** | PR #36 review + 合并（含追加未跟踪 3 文件） | 评审通过 | 文档无版本号，可独立发 |
| **C3** | 抬 xim-pkgindex mcpp → .2 | 与 A5 并行 | **隐藏依赖**：执行计划 §5 第 4 步未明写 |
| **F1** | **已落地**：commit `7e8a17c0` 真实修复——把 musl 依赖收窄到 hosted 目标；CI 重跑中 | 见 §3.2 | 不再算入 open work |

### 4.1 监视策略

- #37 CI 已绿，跳过
- #24 CI 需等 F1 commit 后重跑
- #439 CI `measure (linux, windows through wine)` 待跑，监视

## 5. 第 3 段：索引落地，闸门抬升

| 编号 | 动作 | 触发 | 备注 |
| --- | --- | --- | --- |
| **C1** | mcpp-index#439 draft 转 ready-for-review；附"阻塞解除"说明，引用新 tag 的 sha256 | F1 + A5 + C3 完成后 | 描述里写出 `min_mcpp` 抬升的连锁影响 |
| **C1.5** | 监视 mcpp-index#439 CI（`measure` job 等） | C1 push 后 | Linux 27/3 与基线字符级；Windows 至少 11 个 `_WIN32` 误选消失 |
| **C2** | 合并 mcpp-index#439，发布索引描述文件 | C1.5 通过 | |

### 5.1 C1.5 期望对照（用于判断"通过"）

| 目标 | 基线（0.13） | 本轮期望 | 不通过含义 |
| --- | --- | --- | --- |
| x86_64-linux-gnu | 27 / 3 | 27 / 3（字符级一致） | 闸门误伤，撤回 |
| x86_64-windows-gnu | 15 / 15 | 转绿（具体数字待定）；至少 11 个 `_WIN32` 误选应消失 | 声明未生效 |

## 6. 第 4 段：验证 + 收尾，含设计闸门（B3）

| 编号 | 动作 | 触发 | 备注 |
| --- | --- | --- | --- |
| **B1** | 跑 `.agents/docs/2026-09-18-c-environment-verify.sh` | C2 后 | 沙箱断言零失败 → 写入记录 §4 沙箱段 |
| **B2** | 30 成员兼容性重测（Linux + Windows 两腿） | 在 #439 CI 中跑（`measure` job） | 与 0.13 基线对比；Windows 行预期入册记录 §4 兼容测量段 |
| **B3** | 撤 compat.zlib / compat.mbedtls 适配 | **已在 #439（8fc4b63）** | 闸门：CI 转绿 = 声明生效；CI 失败 = 退回 |
| **B4** | 填记录 §2（各包版本、PR 号、GitHub + GitCode 两端 sha256）、§4 沙箱段、§4 兼容测量段；自审覆盖限制表 6 行 | B1 + B2 + B3 后 | |
| **E1** | 0.13 记录 09-17 归因修订 | **已在 PR #36（6bf6a33）** | 随 #36 merge |

### 6.1 B1 期望输出

```
ok: mcpp 2026.9.18.2
ok: xlings mirror is CN
ok: set and read the executable record; a text file is kal_err_not_program
ok: chmod of the execute bits round-trips through stat and access, a partial mode is ENOSYS, posix_spawn is ENOEXEC
ok: linux: zlib and tinyhttps build above openkal, z_off_t agrees
ok: windows: zlib and tinyhttps cross-build above openkal
ok: the search list of a compat.zlib unit names no host directory
0 assertion(s) failed
```

### 6.2 B2 期望对照（用于判断"通过"）

| 目标 | 0.13 基线 | 期望 | 不通过含义 |
| --- | --- | --- | --- |
| x86_64-linux-gnu | 27 / 3 | 27 / 3（字符级一致） | 闸门误伤 |
| x86_64-windows-gnu | 15 / 15 | 转绿；至少 11 个 `_WIN32` 误选消失 | 声明未生效（B3 设计失败） |

## 7. 设计闸门：B3（已实际在分支）

B3 是 P6 设计判据。commit 8fc4b63 撤掉了 zlib 与 mbedtls 的两条 `-U_WIN32` 适配。判据：

- **通过**（Windows 行 30 成员转绿）：声明 `[c-abi]` 真正生效，写入记录 §3 与设计稿 §3.4 对照
- **不通过**（Windows 行失败）：revert 8fc4b63，停在 mcpp-index#439（不撤适配），退回"逐包适配"路线，回到 0.13 波次的兼容写法

**已实际在分支**——这一闸门变成"CI 自动判定"。如果 #439 CI `measure (linux, windows through wine)` 转绿，则 B3 通过；否则 revert。

## 8. 第 5 段：关波

| 编号 | 动作 | 备注 |
| --- | --- | --- |
| **Z1** | 更新 MEMORY：`openkal-c-environment-wave` 标记 closed，补充 PR 号、最终 sha、限制 6 行；`openkal-0-13-wave` 加新段落 `09-18 c-env close-out` | |
| **Z2** | 在 mcpp / mcp-index / openkal-musl / openkal-llvm-runtime 的 README 中提示索引已发布 + 用户升级命令 | 执行计划 §7 的"可读失败"承诺对用户 |

### 8.1 Z2 的 README 提示措辞

```
mcpp 2026.9.18.2 is required for [c-abi] packages (openkal-musl 0.15.0+).
Older engines silently misbuild them. Upgrade: `xlings install mcpp --force`.
```

## 9. 与"已知不达标项"的边界（6 行；§F 已用真实修复关闭）

| 限制 | 所在层 | 状态 |
| --- | --- | --- |
| macOS xcode-27 两任务（mcpp#669 OPEN；llvm-project#224185 未合并；xim-pkgindex#858 已修一层，第二层换镜像亦不可解析） | runner 镜像 + lld 22.1.8 | 本轮不闭合 |
| install hook 的产物入库不记录环境（结构性，与 c++-abi 同形状 mcpp#613） | mcpp | 本轮不闭合 |
| NASM 汇编无法被告知 C 环境 | mcpp | 本轮不闭合 |
| 第三方库在 `__CYGWIN__` 下找 Cygwin 专有接口（`sys/cygwin.h`、`cygwin_conv_path`）—— 由测量暴露，逐包适配 | 第三方 | 本轮不闭合 |
| `native`（ISO C 形态，picolibc 移植） | 设计 | 按 review 决定推迟 |
| xlings LLVM 默认 sysroot 的两层问题（#858 已修一层，第二层无解） | xlings LLVM 包 | 本轮不闭合 |
| **Windows 主机 × freestanding 目标的 c-abi 探针** | openkal-llvm-runtime CI + clang + mcpp | **已以真实修复**关闭：commit `2570bdf` on `openkal-musl` 给 `os = "none"` 加 `[target.cfg(os = "none").c-abi] presents = "none"`，freestanding 不再承袭 musl 的 hosted c-abi 声明，c-abi 探针无声明可核对，结构性问题随之消失。`musl#37` 5/5 PASS，`llvm-rt#24` 矩阵在跑 |

## 10. 已观察到的执行细节（已更新）

1. **A1/A2/A3/A4/B3/E1 均已在分支上落地**：本计划不是从零写，是清点已落地的工作并标识剩余；详见 §0 与 §3。
2. **xim-pkgindex 的 mcpp 还在 .1**：A5 之后必须并行提 C3，否则用户装的还是 .1。隐藏依赖。
3. **A4 缺 3 个未跟踪文件**（record + verify script + 本 wrap-up plan）：追加一个 commit 即可。
4. **B3 已在分支**：这是设计闸门，但已实际合入 #439 代码里——CI 转绿等于 B3 通过；CI 失败等于 B3 失败。
5. **E1 已在 PR #36（6bf6a33）**：0.13 记录 §4 归因修订随 #36 merge。
6. **新发现 §F**：openkal-llvm-runtime#24 在 Windows 主机 × riscv64-none-elf 上 c-abi 探针失败，根因是 Windows 主机 clang 注入 `_WIN32` + freestanding 不应承袭 `wchar=32`。处置决策点见 §3.2。

## 11. 风险与时间窗（更新）

| 风险 | 概率 | 应对 |
| --- | --- | --- |
| F1 决策点选 (a,a) → 延后关波 2-3 小时 | 中 | 用户可拍板 (c,c) 立即关 |
| #439 `measure` job 失败（B3 设计失败） | 低–中 | revert 8fc4b63；本波关闭时间不变 |
| C3 漏提（xim-pkgindex mcpp 仍在 .1） | 低 | 已在 §0 / §4 显式提醒 |
| 用户升级 mcpp 后遇到 E0006 | 低 | Z2 README 提示 |
| macOS xcode-27 任务影响"全绿"判定 | 高（已知） | 限制表 1 行；本轮不闭合 |