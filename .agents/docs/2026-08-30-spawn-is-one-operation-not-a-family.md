# openkal 0.11:把 spawn 家族收敛成一个操作

> 前提变了:**openkal 还没有外部用户**,处在前期设计与内部验证阶段。所以
> clause 8「不许改已有声明」这条**在 0.x 期间不作为约束**——可以删。
>
> 这一条把结论整个翻过来了。原提案是「加一个通用形式,忍受三个冗余声明」;
> 既然能删,就**没有理由留历史拼法**。

## 1. 现状:同一个操作,三个拼法

| 声明 | 多出来的那件事 |
|---|---|
| `kal_process_spawn` | 基本形 |
| `kal_process_spawn_with` | 交给它一组目录(preopen) |
| `kal_process_spawn_bound` | 生命周期绑在调用者身上 |

这不是三个操作,是**同一个操作的三个修饰**。之所以成了三个声明,唯一的原因是
clause 8 不许给已有声明加参数。

`process.h` 自己写下了这条路的尽头:

> Declaring every combination is how an interface acquires four spawns and then
> eight, so the combination is declared when something needs it and not before.

⚠️ **而「something needs it」已经到了,证据是一个函数签名:**

```
packages/kaos/src/process.cppm:87
    run_shell_stream(argv, cwd, …, timeout_ms)
:112    setpgid(0, 0);                        ← 进程树终止
:113    if (!cwd.empty()) chdir(cwd.c_str()); ← 工作目录
```

相邻两行,同一个子进程。四个修饰互相正交,全组合是 2⁴ = 16,而且**今天就需要
其中一个组合**。

## 2. 设计

```c
/* 一次启动与另一次启动之间会变的东西,全在这里 —— 于是这是一个操作,不是一族。 */
struct kal_spawn {
    struct kal_dir base;               /* `path' 相对谁解析 */
    struct kal_dir work;               /* 程序在哪个目录里跑 */
    const struct kal_preopen* grants;  /* 交给它的目录;可为空,计数为零 */
    kal_uintptr grant_count;
    kal_uintptr flags;                 /* KAL_SPAWN_* */
};

#define KAL_SPAWN_BOUND_LIFETIME ((kal_uintptr)1u << 0)
#define KAL_SPAWN_OWN_JOB        ((kal_uintptr)1u << 1)

int kal_process_spawn(const struct kal_spawn*,
                      const char* path, kal_uintptr path_len,
                      const char** argv, const kal_uintptr* argv_lens, kal_uintptr argc,
                      const char** envp, const kal_uintptr* envp_lens, kal_uintptr envc,
                      const struct kal_spawn_streams* streams,
                      struct kal_process* out);
```

**删掉** `kal_process_spawn_with` 与 `kal_process_spawn_bound`。

⭐ **名字用回 `kal_process_spawn`,不叫 `kal_process_start`。** 能删就没有理由让
接口上同时存在「旧的 spawn」和「新的 start」——一个意思一种拼法。旧调用点会**编译
失败**(参数个数与类型都不同),这是响亮的失败而不是安静的错行为。

### 2.1 ⭐ `work` 是必填的,而这是设计而不是负担

openkal **故意没有环境级的当前目录**——「我碰巧在的那个地方」在这个接口里不是一个
可以指称的东西。所以「程序在哪儿跑」只能由调用者每次说明。

⇒ 一个不在乎的调用者传 `base`。**没有「不说」这个选项**,因为没有一个默认值是真的。

⚠️ 这正好补上 openkal 一直拒绝的那件事的另一半:拒绝「改一个运行中程序的工作目录」
是对的(那是上下文之间的共享可变状态),但拒绝的理由**从来不适用于**「在启动的那一
刻说明它在哪儿跑」——那是每次各说一次的、不可变的、不被共享的。

### 2.2 为什么 `OWN_JOB` 必须是按次选的开关

⚠️ 新进程组会**脱离终端前台组**,带界面的程序里子上下文读终端会拿到 SIGTTIN 停住。
我在 0.10 评估时就是因为这个排除了「让 terminate 一律杀进程组」——**那个理由现在
仍然成立**。三个流全是管道的调用者(跑 shell 的)要这个行为;交互式的绝不要。

⇒ 做成默认行为是错的,做成实现属性也是错的。**只有按次表达才对。**

### 2.3 能力词

`kal_process_props` 保留 `KAL_PROCESS_PROP_GRANT_DIR`、`KAL_PROCESS_PROP_BOUND_LIFETIME`,
新增 `KAL_PROCESS_PROP_OWN_JOB`。

⭐ **`work` 不需要能力位**:三个提供 `openkal.process` 的实现都能做到——Linux/macOS
在替换前 `fchdir`,Windows 用 `CreateProcess` 的 `lpCurrentDirectory`。做不到的
(opensbi/uefi)根本不提供这个接口。

⇒ `KAL_SPAWN_*` 说「调用者要什么」,`KAL_PROCESS_PROP_*` 说「实现能什么」,两个词
不混。

## 3. 全局复核:同类毛病只有这一处

把 `SURFACE.txt` 里所有后缀族都过了一遍:

| 族 | 判定 |
|---|---|
| `kal_env_var` / `_at` | 按名字取 vs 按下标枚举 —— 两个操作 |
| `kal_fs_close_dir` / `_file` | 两种资源类型,签名不同 |
| `kal_fs_open` / `_dir` | 返回 `kal_file` vs `kal_dir` |
| `kal_fs_set_modified` / `_at` | 持有打开的文件 vs 持有名字,等同 `fchmod`/`chmod` |
| **`kal_process_spawn` ×3** | **同一操作的三个修饰 —— 只有这一处** |

⇒ 这次收敛的范围就是 spawn,不扩大。

## 4. 落地顺序

1. **openkal**:`struct kal_spawn` + 合并后的 `kal_process_spawn`,删两个声明;
   SPEC clause 11 记录**删除本身**(为什么删、删了什么);SURFACE 减二;
   `src/process.cppm`;conformance 的 process 节
2. **openkal-linux / macos / windows**:新签名 + `work` + `OWN_JOB`
3. **openkal-opensbi / uefi**:只重钉版本
4. **openkal-musl**:`okm_spawn.c` 换签名,`posix_spawn` 的 `addchdir` 接 `work`、
   `setpgroup` 接 `OWN_JOB`
5. **openkal-llvm-runtime / std-freestanding-alloc-kal**:重钉
6. 用报告者的工程验证:kaos 三条红判据 + agent-core 三条,应当全绿

⚠️ **索引仍然要两批**,和 0.10 一样:alloc-kal 的 CI 从已发布的索引产物解析
openkal,所以它必须等第一批落地、产物刷新之后才能绿。这是真实约束,不是疏漏。
