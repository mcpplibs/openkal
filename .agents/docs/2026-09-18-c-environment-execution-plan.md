# C 环境与形态：执行计划

- 日期：2026-09-18
- 依据：`2026-09-18-openkal-c-environment-and-personalities-design.md`（review 通过，决定 1 到 7，决定 5 推迟）
- 原则：每个仓库一个 PR；尖峰实验先行，数据不通过就停在实验 PR，不进入落地。

## 1. 清单形状（两个 agent 必须一致）

C 库层的包（openkal-musl）声明：

```toml
[package]
provides = ["mcpp:c-abi=musl"]

[c-abi]
presents   = "posix"          # posix | windows | none
data-model = "arch-default"   # arch-default | lp64 | llp64 | ilp32
wchar      = 32               # 16 | 32
builtins   = "iso"            # iso | platform
```

自身在平台环境里编译的包（openkal-windows、平台 shim）声明：

```toml
[package]
c-environment = "platform"
```

引擎在 `kernel-abi` 解析为 `openkal` 时，为目标侧全部单元定义 `__openkal__`。

## 2. 任务与依赖

```
 P0  设计稿补充（已完成）与本计划
 P1  mcpp：环境声明、映射、校验、指纹与 store 键、__openkal__、平台依赖报告与拒绝开关
 P2  openkal-musl：LP64 与 32 位 wchar 的 Windows 头文件，移除四处补丁，声明 [c-abi]
 P3  尖峰实验：P1 的二进制 + P2 的分支，跑探针、conformance、30 个成员、两种呈现对比
 P4  判定：数据通过则继续；不通过则停在实验 PR，退回逐包适配
 P5  落地：mcpp 发布；openkal-musl 0.15.0；openkal-llvm-runtime 0.11.0；openkal 文档修订
 P6  mcpp-index：pins、标签、撤回不再需要的 _WIN32 适配、重新测量
 P7  发布（GitHub 标签 + GitCode 镜像）、沙箱验证、生态级自审
```

P1 与 P2 可以并行；P2 的验证需要 P1 的二进制。

## 3. 判据

| 阶段 | 判据 |
| --- | --- |
| P1 | 单元测试覆盖声明的解析、校验、拒绝；未声明的包命令行逐字节不变；e2e 用一个声明了环境的假包验证映射生效 |
| P2 | 在 P1 的二进制下，musl 为 `x86_64-windows-gnu` 构建通过，探针在 Wine 中全绿 |
| P3 | 30 个成员的测量：Windows 上 15 个失败中转绿的数量；有无新增失败；两种呈现（定义 `__unix__` 与否）的对比 |
| P5 | 各仓库 CI 全绿 |
| P7 | 沙箱中只写版本号即可解析并构建；导入表断言 |

## 4. 风险与退出

- Cygwin 目标的 TLS、异常展开、链接驱动不可用：停在 P4，记录数据，退回逐包适配。
- `__unix__` 带来新的失败超过收益：改为只不定义 `_WIN32`。
- 破坏性变更：openkal-musl 新版本，该目标上的包重建一次；openkal 规范不变。
