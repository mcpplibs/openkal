# 规范仓库的测试：mtest 标准布局（0.25.2，负责人 2026-11-15 确认）

## 结论

| 目录 | 类型 | 内容 |
|---|---|---|
| `tests/unit/` | UTest | 版本：`KAL_VERSION_*` 由编译器读取，与清单版本一致 |
| `tests/e2e/headers.cpp` | E2E | 宏的模块拼写、C 形式在 `-nostdinc` 下编译（cc、clang）、无后端类型及其反证 |
| `tests/e2e/abi.cpp` | E2E | 冻结布局在 riscv32 与 riscv64 上编译（`tests/e2e/data/abi`） |
| `tests/e2e/substitution.cpp` | E2E | 一份源码对两个实现、表面检查拒绝新增名字、接口列表取自产物 |
| `tests/cts/` | CTS、Bench | 一致性套件（原 `conformance/`），独立成员包 |
| `kit/tests/` | UTest | openkal-kit 的用例，随包走 |

## 约束与取舍

- **cts 不使用 mtest 的 C++ 库。** 库依赖 `import std`，而套件要在没有 C 库的实现上运行（固件、裸机、DOS）。
  套件自己实现协议（L2：cts 用例、bench 用例、选择、列出）。bench 留在套件内，与各节共享状态，所有平台都有成本数据。
- **检查用 C++ 驱动工具。** `tools/` 下的脚本同时供实现仓库使用，保持不变。用例在自己的临时目录里操作副本
  （头文件副本、示例副本、生成到临时文件的 `macros.cppm`），不写检出目录，因此无需串行。
  缺少的编译器作为能力（capability）声明，相应用例跳过并给出原因。
- **CI 收敛。** `tests` 作业在 Linux（gcc、llvm）与 macOS（llvm）上执行 `mtest run --set full`；
  `declarations` 作业在五行上只构建模块形式。这些用例的对象是文本、头文件和工具，与消费者使用的工具链无关，
  所以不必在每一行上运行。
- **套件是独立的 mtest 项目（0.25.3）。** `tests/cts/mtest.toml` 使引擎在套件目录运行程序并把运行记录存入 `tests/cts/.mtest`。
  0.25.2 缺少该文件，引擎向上取仓库根的 `mtest.toml`：记录写入根存储，交给运行器（Web 行的 `node`）的相对程序名按根目录解析而失败。
- **兼容。** `tools/run-conformance.sh` 的参数不变，实现仓库的调用方式不变。套件的存储变为
  `tests/cts/.mtest`；读取该存储的实现仓库 CI（linux、macos、windows、posix、emscripten）和 freebsd 的
  独立运行脚本在本版合入后同步路径。

## 生态侧发现

- mcpp：默认的 `tests/**/*.cpp` 会把成员包 `tests/cts/src/*.cpp` 当作根包测试。mcpp 2026.11.14.1 规定，
  包根之下含 `mcpp.toml` 的目录是另一个包，不参与发现。本仓库显式声明 `[test] discover = ["tests/*/*.cpp"]`，
  不依赖该版本。CI 的 mcpp 固定为 2026.11.14.1。
- mtest 0.8.5：库在 macOS 的 LLVM 20 libc++（`import std`）下无法编译（`lib/src/mtest.cpp` 中
  `directory_iterator` 的比较未经 std 模块导出）。`tests` 作业的 macOS 行因此使用 LLVM 22。
