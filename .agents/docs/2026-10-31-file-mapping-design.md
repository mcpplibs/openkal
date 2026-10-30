# 文件映射：`openkal.map`

第一版，2026-10-31。状态：**实施所依据的方案**。规范对照基于 `SPEC.md` 0.17.1，结果为 0.18.0。
来源：mcpp 生态设计 2026-10-30 part 2 §19.8、§23.2、§25（openkal 只补文件映射；clangd 的 ok 映像，mcppls-clangd#12）。

---

## 1. 要解决的问题

| 消费者 | 今天 | 需要 |
| --- | --- | --- |
| ok 映像中的 LLVM（clangd） | 对文件的 `mmap` 得到 `ENOSYS`，回退为整文件读入；页不共享 | 只读映射，按需取页 |
| okrun | 把整个映像文件按倍增读进堆，运行期间不释放（71.5 MB 的映像占 128 MiB） | 映射映像文件 |
| openbus 的共享区域、进程间的大数据 | 无 | 两个进程映射同一文件，写入互见 |
| 嵌入大数据并映射（llamafile 式） | 无 | 在 64 KiB 边界上映射文件的一段 |

三者都不能由已有的原子组合：读入是复制，不共享，也不按需；`openkal.memory` 与 `openkal.exec` 都没有"一个文件的内容"
这一资源。

## 2. 准入（0.8 的准则）

一个接口进入规范，当且仅当：它是每个内核都有的最小能力；不能由已规定的原子组合；两个方向都可用 —— 上方可组合，下方
可实现。

- 每个内核：Linux、macOS 的 `mmap`，Windows 的 `CreateFileMapping` + `MapViewOfFile`；三者都是一次调用，无需表或
  名字解析（条款 7.1）。
- 不可组合：见 §1。
- 上方：openkal-musl 的 `mmap`、`munmap`、`msync`；okrun；openbus。下方：三个实现，各十几行。

## 3. 形状

### 3.1 一个独立的可选接口

`openkal.map`，资源：**一个文件的一段在地址空间中的映像**。不并入 `openkal.fs` 或 `openkal.space`：

- 条款 6.2：实现可能缺少的操作自成接口。有文件而不能映射的环境是存在的 —— openkal-emscripten 的文件在线性内存中，
  没有 MMU 的环境（openkal-opensbi、openkal-uefi）有块设备上的文件而没有页表。若并入 `openkal.fs`，它们就不能再提供
  `openkal.fs`。
- `openkal.space` 的资源是地址空间与在其中执行的上下文；一个能复制地址空间而没有文件的环境若因此不能提供
  `openkal.space`，是把两个资源绑在了一起。

`openkal.map` 以 `openkal.fs` 的 `struct kal_file` 为输入，因此依赖 `openkal.fs`（conformance 的特性 `map = ["fs"]`）。

### 3.2 声明

```c
#define KAL_MAP_ALIGNMENT ((kal_u64)65536u)

#define KAL_MAP_WRITE  ((kal_uintptr)1u << 0)
#define KAL_MAP_SHARED ((kal_uintptr)1u << 1)

int  kal_map_file(struct kal_file file, kal_u64 offset, kal_uintptr length,
                  kal_uintptr how, void** out);
int  kal_map_sync(void* region, kal_uintptr length);
void kal_map_release(void* region, kal_uintptr length);
```

三个操作，一个常量，两个标志位。没有句柄、没有 props 字、没有粒度查询。

| 决定 | 理由 |
| --- | --- |
| 区域以地址与长度表示，不设句柄 | 与 `kal_alloc`/`kal_free`、`kal_exec_alloc`/`kal_exec_free` 同形。Windows 的映射对象在 `MapViewOfFile` 之后即可关闭，视图使其存活；POSIX 的 `munmap` 需要长度，调用者本来就知道。设句柄则实现要维护"句柄 → 视图"的表（条款 7.1 所禁止的兼容层） |
| `kal_map_file` 返回 `int` 并经指针写出地址 | 条款 5.2.1：产生资源的操作。失败的原因有多种（对齐、越界、权限、卷不支持），调用者据以行动 |
| `kal_map_release` 返回 `void` | 释放一个有效区域在三个环境上都不失败（`munmap`、`UnmapViewOfFile`）；与 `kal_free`、`kal_exec_free` 同形 |
| 偏移是常量 `KAL_MAP_ALIGNMENT`（64 KiB）的整数倍，而不是查询粒度 | 为映射而写的文件（嵌入的数据、ok 映像的段、单文件的 stored zip）在写出时就要知道对齐，而写出它的程序不知道它将在哪个环境中被映射 —— 条款 6.5 第二段的"产物分发时规则反转"。64 KiB 是所针对的环境中粒度的最大者（Windows 的分配粒度），条款 12.2 的映像段与 SPEC-013 的 stored zip 已用同一个数。粒度不整除 64 KiB 的环境不提供本接口（条款 6.1），而不是在常量上撒谎 |
| `length` 任意正数，`offset + length` 不超过调用时文件的长度 | Windows 的视图不能越过映射对象，映射对象不能越过文件（除非扩展文件，而映射不应改变文件的长度）；POSIX 允许越过，但越过的部分访问即故障。取两者的交集：不越过。扩展文件用 `kal_fs_truncate` |
| 只读或可写；私有或共享 | `access × sharing` 四种组合在三个环境上都是一次调用：`PROT_READ[|PROT_WRITE]` × `MAP_PRIVATE|MAP_SHARED`；`PAGE_READONLY`/`PAGE_READWRITE`/`PAGE_WRITECOPY` × `FILE_MAP_READ`/`FILE_MAP_WRITE`/`FILE_MAP_COPY` |
| 没有可执行的映射 | 执行代码只经 `kal_exec_*`（W^X 由该接口保证）。§19.1 的 probe 中 Windows 拒绝以执行权限映射一般文件 |
| `kal_map_sync` 对整个区域，不接受子范围 | `msync` 要求页对齐的起点，而页大小不是本接口的事实；整个区域在三个环境上都是一次调用 |

### 3.3 语义

1. **文件的打开方式。** 区域可读要求文件以 `KAL_OPEN_READ` 打开；`KAL_MAP_WRITE | KAL_MAP_SHARED` 还要求
   `KAL_OPEN_WRITE`；否则 `kal_err_permission`。私有的可写区域只要求可读（写入是写时复制，不回到文件）。
2. **参数。** `offset` 不是 `KAL_MAP_ALIGNMENT` 的倍数、`length` 为零、`offset + length` 超过文件的长度、`how` 含未
   定义的位：`kal_err_invalid`。文件所在的卷不能映射（设备、合成的文件）：`kal_err_not_supported` —— 这是资源的性质
   （同 `openkal.fs` 中按卷回答的先例），不是实现缺少操作。
3. **区域的生存期。** 区域在 `kal_map_release` 之前一直有效，与文件句柄是否已关闭无关。`kal_map_release` 的参数是
   `kal_map_file` 给出的地址与传入的长度，二者都不得是区域的一部分。
4. **末页。** 区域到达文件末尾时，自文件末尾起到距 `offset` 为 4096 的下一个倍数为止的字节可读，且为零。4096 是所针对
   的环境中页的最小者，每个环境的页都是它的倍数，故这些字节总在已映射的末页内。一个以末尾零字节为终止符读取文件的
   程序（LLVM 的 `MemoryBuffer`）依赖这一点。
5. **共享区域的可见性。** 一个文件的各个共享区域（任一进程的）之间，写入立即可见。共享区域的写入在 `kal_map_sync`
   返回后对经 `openkal.fs` 读取该文件可见。经 `openkal.fs` 写入的字节何时出现在已有的区域中未规定。`kal_map_sync`
   不使写入免于机器的掉电 —— `openkal.fs` 对经流写入的字节同样不作此陈述。私有区域上的 `kal_map_sync` 不做任何事，
   报告 `kal_ok`。
6. **私有区域。** 写入只对本区域可见。文件此后的改动是否出现在尚未写过的页中未规定。
7. **文件被截短。** 一个区域存在时，把文件截短到区域末尾之前：或者截短被拒绝（Windows），或者截短成功，此后访问整页
   位于新末尾之后的字节是一次故障，程序以故障终止（Linux、macOS）。新末尾所在页中、末尾之后的字节为零。实现**不得**
   以零页代替被截去的内容。访问是内存读写，没有可返回的错误值，故以故障报告；一个可移植的程序不截短它映射着的文件。
8. **并发。** 条款 6.6 照常适用：不同区域上的操作可以并发；同一区域上并发的 `kal_map_sync` 与 `kal_map_release` 的
   结果未规定。

### 3.4 条款 4.4 的六条

| 条 | 本接口 |
| --- | --- |
| 1 不导出对象 | 是 |
| 2 不返回指向实现存储的指针 | 区域在调用者的地址空间中，是调用者的存储，不是实现的（与 `kal_alloc` 相同） |
| 3 结果不宽于一个机器字 | `int`、`void` |
| 4 结构携带大小或说明为何不能增长 | 不引入结构；`struct kal_file` 已冻结 |
| 5 入口 | 不涉及 |
| 6 缺席可由操作发现 | `KAL_IFACE_MAP`（`kal_interfaces` 的第 15 位） |

S、L、X 三列都为 ✓：一个在另一地址空间中的实现（X）映射调用者的地址空间，正是内核的 `mmap`。

### 3.5 映像（条款 12）

`kal_map_*` 是规范声明的名字，映像可以导入。条款 12.2：弱导入在实现不提供时为零。openkal-musl 以弱引用导入它们，
故条款 12.4 的必需接口不含 `openkal.map`：一个使用 C 库 `mmap` 的映像在旧的 okrun 上照常运行，文件映射回退为
`ENOSYS`（LLVM 再回退为读入）。加载器为新名字生成的调用约定转换来自 `SURFACE.txt`（条款 12.3），openkal-image 随
新版本重新生成。

## 4. 未采纳

| 方案 | 不采纳的原因 |
| --- | --- |
| 并入 `openkal.fs` | §3.1：有文件而不能映射的环境会因此失去 `openkal.fs` |
| 并入 `openkal.space`（part 2 rev 6 的写法 `kal_space_map_file`） | §3.1：把两个资源绑在一起 |
| 句柄形式（`struct kal_mapping`） | 实现要维护表；§3.2 |
| `kal_map_granularity()` 查询 | 写出文件的一方需要对齐时已知（§3.2）；查询的值在不同环境不同，以它对齐写出的文件在别处不能映射 |
| 可执行的文件映射 | W^X 只经 `openkal.exec`；Windows 拒绝 |
| 匿名共享内存操作 | "授权目录下的文件 + 共享映射"已覆盖，且名字可由目录授权约束（条款 7.13）；匿名共享需另一种命名与授权 |
| 子范围的 `kal_map_sync` 与部分释放 | 需要页大小；Windows 只能释放整个视图 |
| 映射时扩展文件 | 映射不应改变文件；用 `kal_fs_truncate` |
| props 字（例如"截短被拒绝"） | 可移植程序不截短映射着的文件，两种行为都不读到错误的字节，没有程序据此行动（条款 5.2 的准则 1） |

## 5. 实现

| 仓库 | 版本 | 内容 |
| --- | --- | --- |
| openkal | 0.17.1 → 0.18.0 | `map.h`、`openkal.map` 模块、`SURFACE.txt`、`KAL_IFACE_MAP`、条款 3 的表与条款 11 第 31 条、conformance 的 `map` 节与特性 |
| openkal-linux | 次版本 | `mmap`/`munmap`/`msync(MS_ASYNC)` 系统调用；`fstat` 取长度 |
| openkal-macos | 次版本 | 同上（BSD 系统调用号） |
| openkal-windows | 次版本 | `CreateFileMappingW`（大小 0）+ `MapViewOfFile`，随即关闭映射对象；`UnmapViewOfFile`；`FlushViewOfFile`；`ERROR_USER_MAPPED_FILE` 映射到 `kal_err_permission` |
| openkal-image | 次版本 | 绑定表含新名字；okrun 以 `kal_map_file` 读入映像与 okb，映射不成时回退为读入 |
| openkal-musl | 次版本 | 文件 `mmap`（`PROT_EXEC` 拒绝；偏移按 64 KiB 向下对齐，返回区域内的地址）；`munmap`、`msync` 经一张区域表（C 库重建 POSIX，表属于 C 库） |
| openkal-llvm-runtime、openkal-platform、mcpplibs.os | 次版本或补丁 | 精确钉住 |

openkal-emscripten、openkal-opensbi、openkal-uefi 不提供本接口，不动。

## 6. 一致性测试（conformance 的 `map` 节）

1. 只读：映射一个已知内容的文件，逐字节一致；末页中文件末尾之后到 4096 的倍数为零。
2. 私有可写：写入可见于本区域，文件内容不变（经 `openkal.fs` 读取）。
3. 共享可写：写入后 `kal_map_sync`，经 `openkal.fs` 读到写入；同一文件的第二个共享区域立即看到写入。
4. 拒绝：偏移不对齐、长度为零、越过末尾、未定义的位 → `kal_err_invalid`；以只读打开的文件请求共享可写 →
   `kal_err_permission`。
5. 生存期：关闭文件句柄后区域仍可读。
6. 截短：区域存在时截短文件；被拒绝则通过，成功则在子进程中访问被截去的页，子进程以故障终止（不是正常退出）。
7. `KAL_IFACE_MAP` 被报告；SURFACE 与声明的检查（条款 9 的第二、三、四半）。
