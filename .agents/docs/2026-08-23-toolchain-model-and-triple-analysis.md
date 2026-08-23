# 三个问题的分析:depfile 的 awk、openkal-llvm 的定位、三元组的语义

**日期**:2026-08-23
**问题来源**:review 之后提出的三问 —— (1) 把 awk 移植到 openkal 上放进
xim-pkgindex 能不能解决 Windows 宿主的 include 依赖追踪;(2) `openkal-llvm` 与
`llvm` 在工具链模型里是什么关系,它到底是什么;(3) `arch-os[-env]` 这套三元组的
语义,openkal 归属到哪里。

---

## 1. depfile:awk 不是这里缺的东西

### 1.1 ⭐⭐ 前提被代码自己推翻

`ninja_backend.cppm` 里的两个判据:

```c++
const bool posixDepfile      = !msvcDeps && !mcpp::platform::is_windows;
const bool needsGnuModuleFilter =
    posixDepfile && plan.toolchain.compiler == CompilerId::GCC;
```

`awk` 只出现在 `needsGnuModuleFilter` 这一支,而那一支**只对 GCC 成立**。它的用途
是剥掉 GCC 在 depfile 里额外写的模块反向规则。同一段注释记着当时的实测:

```
clang:  x.o: x.cppm ops.inc                          — 一条普通规则
gcc:    x.o gcm.cache/x.gcm: x.cppm ops.inc
        x.c++-module: gcm.cache/x.gcm + .PHONY + gcm.cache/x.gcm:| x.o
```

> So Clang emits nothing the filter would need to remove.

⇒ **clang 根本不需要那个过滤器,也就不需要 awk。**

而关掉 depfile 的那个判据是 `!is_windows` —— 一个**关于宿主**的检查,站在
「awk 可能不在」的位置上,却同时关掉了**不需要 awk 的 clang**。

### 1.2 ⚠️ 这是 #257 那个错误换了一根轴复发

同一个文件里写着 #257 的教训:

> #257: these are TWO decisions, and 0.0.97 conflated them. … Gating the first
> on the second left Clang with no include tracking for four releases.

「发不发 depfile」和「要不要剥掉 GCC 的反向规则」是两个决定。0.0.97 把第一个挂在
第二个上,#257 拆开了。⚠️ **而现在第一个又被挂在了「宿主是不是 Windows」上** ——
一个 GCC 形状的细节,再一次决定了一个与它无关的正确性契约。

### 1.3 移植 awk 能解决吗:能,但它是错的形状

对 **Windows + mingw-gcc** 这一格,一个可用的 awk 确实能让过滤器跑起来。但:

1. ⚠️ **它对你实际遇到的那一格无效** —— 你看到那条警告是在 **Windows 宿主 + clang**
   的 3×3 构建里,而那一格从来不需要 awk。
2. ⚠️ **它把构建工具的一条正确性契约变成了一个包的安装状态**。「编辑一个被 include
   的文件会不会触发重建」不该取决于索引里有没有装上某个东西 —— 没装时的表现是
   **静默复用陈旧的 BMI**,而不是一条报错。
3. ⚠️ 它给 mcpp 加了一条新的宿主依赖,而这套生态一路在做的是相反的事(openkal-windows
   自己声明 Win32、自己生成导入库,就是为了不依赖机器上有什么)。

### 1.4 建议的形状:拆判据,并让 mcpp 做自己的过滤器

| 组合 | 需要什么 | 今天 | 应当 |
|---|---|---|---|
| 任意宿主 + clang | 只要 `-MMD -MF` | Windows 上被关掉 | **开启,零新增依赖** |
| POSIX 宿主 + gcc | `-MMD` + 过滤器 | 已开启(awk) | 不变,或改用下面那条 |
| Windows + mingw-gcc | `-MMD` + 过滤器 | 关闭 | 过滤器不用 awk |
| 任意宿主 + cl.exe | `/showIncludes` | 已开启(`deps = msvc`) | 不变 |

⭐ 第三行的过滤器可以完全不需要外部工具:**`mcpp` 自己就在那台机器上**,它可以提供
一个内部子命令做这件事(读 `$out.d.raw`,只保留第一条记录)。那是十几行代码,消掉
的是一条**在每个平台上都存在**的外部依赖 —— 包括 POSIX 上对 awk 的依赖。

⇒ **结论:不要移植 awk。第一步(拆判据)就解决了你遇到的那一格,第二步(mcpp 自带
过滤器)顺带把 POSIX 上的 awk 依赖也去掉。**

---

## 2. `openkal-llvm` 是什么

### 2.1 实测:它和 `llvm` 是同一个载荷

`registry.cppm`:

```c++
if (spec.family == Family::Llvm || spec.family == Family::OpenkalLlvm) {
    pkg.ximName = mcpp::toolchain::llvm::package_name();   // 同一个包
    pkg.frontendCandidates = mcpp::toolchain::llvm::frontend_candidates();
    return pkg;
}
```

同一个 xim 包、同一个 `clang++`、同一个版本号。`toolchain list --available` 把它
列两行,并注明「装其中任何一个就装了两个」。

⇒ **它不是一个工具链**,不是一个编译器,也不是一个 C++ 运行时(那是
`openkal-llvm-runtime`,一个普通的依赖包)。

### 2.2 它实际断言的是:**目标侧从依赖图来**

唯一的差别在 `prepare.cppm`:

```c++
const bool openkalTargetSide = [&] {
    auto family_of = [](std::string_view spec) { … return fam == "openkal-llvm"; };
    …
}();
```

它绕过「这台宿主建不了那个目标」的拒绝 —— 因为在 openkal 下,「有没有载荷能产出这个
目标」不再是正确的问题;正确的问题是「有没有实现」,而那个问题由 openkal 的
clause 6.1 在**链接期**回答,报出解析不了的 `kal_*`,比拒绝更准确。

### 2.3 ⚠️ 为什么它长在「工具链」这个字段上:一个**时序**理由,不是语义理由

那条拒绝发生在**依赖解析之前**。而「目标侧来自图」这件事,mcpp 本来是知道的 ——
`targetCxxRuntime` 由图里某个包的 `provides = ["hosted-standard-library"]` 推出 ——
但那时候还不知道。

⇒ 所以用户被要求在 `[toolchain]` 里说一件**关于依赖图**的事,只是因为那个字段是在
正确的时刻能被读到的少数几个之一。

### 2.4 ⚠️ 由此产生的三处不一致(你观察到的那个)

| 位置 | 写的是 | 为什么也能工作 |
|---|---|---|
| `openkal-llvm-runtime/mcpp.toml` | `llvm@22.1.8` | 它作为**依赖**被构建时,`[toolchain]` 被忽略 —— 只有根包的算数 |
| `examples/cxx`、`examples/import-std` | `llvm@22.1.8` | 它们是**本机**构建,没有 `--target` ⇒ `crossTargetFlag` 为空 ⇒ 两者等价 |
| `examples/same-source` | `openkal-llvm@22.1.8` | 它要交叉,那是唯一有差别的情形 |

⇒ **`openkal-llvm` 只在给了 `--target` 时才改变任何东西**(`graphTargetSide =
targetCxxRuntime && !crossTargetFlag.empty()`)。三处写法不同而三处都正确,是因为
另外两处**处在这个区别不存在的情形里** —— 这不是笔误,但它读起来像笔误,而一个读起来
像笔误的正确写法是一种设计债。

### 2.5 ⚠️ 两处死代码 / 两条通道

1. `family_serves_every_target(Family)` —— **定义了,零调用点**。
2. `Family::OpenkalLlvm` 这个枚举只在 `registry.cppm` 内部被用到;真正的门是
   `prepare.cppm` 里对**清单文本**的字符串比较 `fam == "openkal-llvm"`。

⇒ 同一个事实有一个枚举和一个字符串两种表示,而只有字符串那条承重。

### 2.6 三条可能的形状

| 方案 | 做法 | 代价 |
|---|---|---|
| **A. 保持现状,补齐一致性** | 四份清单统一写 `openkal-llvm`;删掉死代码;把枚举和字符串收敛成一处 | 语义仍然错位(拿工具链字段说图的事),但**零风险**,且读起来不再像笔误 |
| **B. 移出工具链字段** | 新增 `[build] target-side = "graph"`(或让 `provides` 提前可见),`[toolchain]` 只写 `llvm` | 说的是对的事;⚠️ 要么把拒绝推迟到解析之后,要么在解析前先看一眼清单的 `[dependencies]` |
| **C. 让它完全消失** | 拒绝改为:先解析依赖,若图里有 `hosted-standard-library` 就不拒绝 | ⭐ 最干净 —— 用户什么都不用写;⚠️ 改变解析顺序,影响面最大,且「这台宿主建不了那个目标」的报错会变晚 |

⭐ **推荐 A 立即做,C 作为方向**。B 是把一个字段换成另一个字段,收益不抵改动面。

---

## 3. 三元组的语义

### 3.1 ⚠️ 第三个字段今天表示三件不同的事

| 三元组 | 第三段 | 它实际在说 |
|---|---|---|
| `x86_64-linux-gnu` | `gnu` | **C 库**(glibc) |
| `x86_64-linux-musl` | `musl` | **C 库** |
| `x86_64-windows-gnu` | `gnu` | **ABI / C 运行时**(MSVCRT via mingw),不是 C 库 |
| `x86_64-windows-msvc` | `msvc` | **ABI / C 运行时** |
| `riscv64-none-elf` | `elf` | **目标格式** |
| `aarch64-macos` | (无) | —— |

⇒ 同一个位置,三种含义加一个空缺。这是从 LLVM 继承来的,不是 mcpp 造的,但 mcpp 的
词表把它当成一个统一的「env」在用。

### 3.2 ⭐⭐ 而 openkal 让它变成了一个**不准确的陈述**

实测,`mcpp build --target x86_64-linux-gnu`(over openkal):

| | |
|---|---|
| 产物 | 静态 ELF,x86-64 |
| glibc 符号 | **0** |
| musl 痕迹 | 4102 |
| `kal_*` 符号 | 48 |

⇒ **三元组说 `-gnu`,而底下是 musl-over-openkal。** 同一个三元组现在指两种完全不同的
栈,而那正是第三个字段本该区分的东西(它对 `gnu`/`musl` 就是这么用的)。

### 3.3 三种可能的归属,以及它们各自的问题

**方案甲:openkal 进第三段** —— `x86_64-linux-openkal`

- ✅ 与 `gnu`/`musl` 的既有用法一致:第三段说「C 库是什么」。
- ⚠️ 但 openkal **不是一个 C 库** —— 它是 C 库下面的那一层。`x86_64-linux-openkal`
  仍然没说 C 库是 musl 还是 picolibc。
- ⚠️ 而且它会让 `x86_64-windows-openkal` 与 `x86_64-windows-gnu` 并列,而那两段说的
  又不是同一类东西。

**方案乙:三元组只说机器,栈由图说** —— 维持现状

- ✅ 与 openkal 的整个立场一致:openkal 不改变架构、不改变目标格式、不改变调用约定;
  改变的是**下面是谁**,而那是包的事。
- ✅ 也与 mcpp 已经做的事一致:`format_for` 只问 arch/os,`graph_runtime_compile_flags`
  只问格式,两者都不需要知道 openkal。
- ⚠️ 代价就是 §3.2 那条:`-gnu` 在 openkal 下是**假的**,因为第三段对 Linux 恰好被
  用来说 C 库。

**方案丙:把第三段的含义收敛,再谈 openkal**

⭐ 真正的病灶是第三段一位多义。若把它明确为**「ABI/环境」**而不是「C 库」:

| | 第三段 | openkal 在哪 |
|---|---|---|
| `x86_64-linux-gnu` | Linux 的 syscall ABI | 图里 |
| `x86_64-windows-msvc` / `-gnu` | Windows 的两种 ABI | 图里 |
| `riscv64-none-elf` | 无环境 | 图里 |

- ✅ 那么 `gnu` 在 Linux 上说的是「Linux 的用户态 ABI」而不是「glibc」,`-gnu` 就不再
  是假的 —— openkal-linux 发的正是那套 syscall。
- ✅ `musl` 就成了那个真正的异类:它是唯一一个用第三段说 C 库的,而它之所以需要,是因为
  glibc/musl 在**同一套 ABI 上**给出不同的产物形状(动态/静态、PT_INTERP)。
- ⚠️ 这需要重新审视 `x86_64-linux-musl` 这一行到底在说什么;它可能应该是一个
  **linkage/分发**属性而不是三元组的一段。

### 3.4 建议

⭐ **短期(与本轮改动一致):方案乙 + 把这条不准确写进文档。** 三元组说机器,图说栈;
并在 `[target.…]` 的文档里写明「第三段对 Linux 历史上被用来说 C 库,而 openkal 之上
它不再成立」。这是零风险的诚实。

⭐⭐ **中期:方案丙。** 把第三段的含义收敛成「ABI/环境」,并把 `musl` 这一行重新定位。
判据是可以机器核验的:**同一个三元组的两次构建,产物的外部依赖集合应当相同** ——
今天 `x86_64-linux-gnu` 在 openkal 上和在 glibc 上,这个集合完全不同,而那就是三元组
在说谎的定义。

⚠️ **不建议方案甲。** 把 openkal 塞进第三段,是把一个「下面是谁」的答案塞进一个已经
一位多义的字段,会让那个字段变成四义。

---

## 4. 三问的共同结构

三个问题各自的答案不同,而它们指向同一件事:

> **一个字段/判据被用来回答它不是为之而设的问题,而它之所以被这么用,是因为它恰好
> 在那个时刻可用。**

- `!is_windows` 被用来回答「awk 在不在」,而真正的问题是「编译器是不是 GCC」。
- `[toolchain]` 被用来回答「目标侧从哪来」,而它是因为在依赖解析前可读才被选中。
- 三元组的第三段被用来回答「C 库是什么」「ABI 是什么」「格式是什么」三个问题。

⇒ 每一处的修法都是同一句话:**把问题问回到知道答案的那一层**。而本轮在 mcpp 侧做的
九处修复,全部是这句话的实例。
