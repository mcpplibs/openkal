// Probe (mcpp#849): the primitives an openkal implementation over
// Cosmopolitan's libc (openkal-cosmo) would rest on, measured on every
// system and architecture the APE runs on. One JSON line per check:
//   {"area":"exec","check":"rw-then-rx","ok":true,"detail":"..."}
// The program never fails on a check; the summary line counts them.
// Temporary: this file lives on a probe branch and is not merged.
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#ifdef __COSMOPOLITAN__
#include <cosmo.h>
#endif

extern char** environ;
static int g_ok = 0, g_fail = 0;
static char g_dir[4096];   // the scratch directory

static void json_str(const char* s) {
    putchar('"');
    for (; s && *s; ++s) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { putchar('\\'); putchar(c); }
        else if (c < 0x20) printf("\\u%04x", c);
        else putchar(c);
    }
    putchar('"');
}

static void report(const char* area, const char* check, int ok, const char* fmt, ...) {
    char detail[1024] = "";
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(detail, sizeof detail, fmt, ap);
        va_end(ap);
    }
    printf("{\"area\":"); json_str(area);
    printf(",\"check\":"); json_str(check);
    printf(",\"ok\":%s,\"detail\":", ok ? "true" : "false"); json_str(detail);
    printf("}\n");
    fflush(stdout);
    if (ok) ++g_ok; else ++g_fail;
}

static void path_in(char* out, size_t n, const char* name) { snprintf(out, n, "%s/%s", g_dir, name); }

// ── env ─────────────────────────────────────────────────────────────────
static void probe_env(int argc, char** argv) {
    struct utsname u;
    int r = uname(&u);
    const char* sys =
#ifdef __COSMOPOLITAN__
        IsWindows() ? "windows" : IsXnu() ? "macos" : IsLinux() ? "linux" : IsFreebsd() ? "freebsd" :
        IsOpenbsd() ? "openbsd" : IsNetbsd() ? "netbsd" : "other";
#else
        "native";
#endif
    const char* arch =
#if defined(__x86_64__)
        "x86_64";
#elif defined(__aarch64__)
        "aarch64";
#else
        "other";
#endif
    report("env", "system", r == 0, "sys=%s compiled-arch=%s uname=%s %s %s", sys, arch, u.sysname, u.release, u.machine);
    report("env", "argv", argc >= 1 && argv[0] != NULL, "argc=%d argv0=%s", argc, argv[0]);
    int n = 0;
    for (char** e = environ; e && *e; ++e) ++n;
    report("env", "environ", n > 0, "%d entries", n);
#ifdef __COSMOPOLITAN__
    const char* exe = GetProgramExecutableName();
    report("env", "executable", exe && *exe, "%s", exe ? exe : "(null)");
#endif
    char cwd[4096];
    report("env", "getcwd", getcwd(cwd, sizeof cwd) != NULL, "%s", cwd);
}

// ── exec: executable memory (kal_exec_alloc/publish; okrun maps images) ──
typedef int (*fn0)(void);
typedef int (*fn1)(int);
#if defined(__x86_64__)
typedef int (__attribute__((sysv_abi)) *fn1_sysv)(int);
typedef int (__attribute__((ms_abi)) *fn1_ms)(int);
#endif

static void probe_exec(void) {
    long page = sysconf(_SC_PAGESIZE);
    report("exec", "page-size", page > 0, "%ld", page);
#if defined(__x86_64__)
    static const unsigned char ret42[] = {0xB8, 0x2A, 0, 0, 0, 0xC3};             // mov eax,42; ret
    static const unsigned char inc_rdi[] = {0x8D, 0x47, 0x01, 0xC3};               // lea eax,[rdi+1]; ret
    static const unsigned char inc_rcx[] = {0x8D, 0x41, 0x01, 0xC3};               // lea eax,[rcx+1]; ret
#elif defined(__aarch64__)
    static const uint32_t ret42[] = {0x52800540, 0xD65F03C0};                      // mov w0,#42; ret
    static const uint32_t inc_rdi[] = {0x11000400, 0xD65F03C0};                    // add w0,w0,#1; ret
#endif
    // RW, write, then RX (W^X): what a loader of images does.
    void* p = mmap(NULL, (size_t)page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) { report("exec", "rw-then-rx", 0, "mmap rw: %s", strerror(errno)); return; }
    memcpy(p, ret42, sizeof ret42);
    __builtin___clear_cache((char*)p, (char*)p + sizeof ret42);
    if (mprotect(p, (size_t)page, PROT_READ | PROT_EXEC) != 0) {
        report("exec", "rw-then-rx", 0, "mprotect rx: %s", strerror(errno));
    } else {
        int v = ((fn0)p)();
        report("exec", "rw-then-rx", v == 42, "returned %d", v);
    }
    // RWX at once (what a loader must avoid where W^X is enforced).
    void* q = mmap(NULL, (size_t)page, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    report("exec", "rwx-mapping", q != MAP_FAILED, q == MAP_FAILED ? "refused: %s" : "allowed", strerror(errno));
    if (q != MAP_FAILED) munmap(q, (size_t)page);
    // The calling convention of code this program calls (an image's imports
    // are SysV / AAPCS): does a default-ABI call pass the first int in rdi/w0?
    void* r = mmap(NULL, (size_t)page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (r != MAP_FAILED) {
        memcpy(r, inc_rdi, sizeof inc_rdi);
        __builtin___clear_cache((char*)r, (char*)r + sizeof inc_rdi);
        if (mprotect(r, (size_t)page, PROT_READ | PROT_EXEC) == 0) {
            int dflt = ((fn1)r)(41);
            report("exec", "default-abi-is-sysv", dflt == 42, "default call of a SysV/AAPCS callee returned %d", dflt);
#if defined(__x86_64__)
            int sysv = ((fn1_sysv)r)(41);
            report("exec", "explicit-sysv-call", sysv == 42, "returned %d", sysv);
#endif
        }
        munmap(r, (size_t)page);
    }
#if defined(__x86_64__)
    void* m = mmap(NULL, (size_t)page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (m != MAP_FAILED) {
        memcpy(m, inc_rcx, sizeof inc_rcx);
        if (mprotect(m, (size_t)page, PROT_READ | PROT_EXEC) == 0) {
            int ms = ((fn1_ms)m)(41);
            report("exec", "explicit-ms-abi-call", ms == 42, "returned %d", ms);
        }
        munmap(m, (size_t)page);
    }
#endif
    munmap(p, (size_t)page);
}

// ── space: file mapping (mcppls-clangd#12: openkal has no file mmap) ────
static void probe_space(void) {
    char f[4200];
    path_in(f, sizeof f, "map.bin");
    int fd = open(f, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { report("space", "file-map", 0, "open: %s", strerror(errno)); return; }
    const char text[] = "mapped bytes";
    write(fd, text, sizeof text);
    void* ro = mmap(NULL, sizeof text, PROT_READ, MAP_PRIVATE, fd, 0);
    report("space", "file-map-private-read", ro != MAP_FAILED && memcmp(ro, text, sizeof text) == 0,
           ro == MAP_FAILED ? "%s" : "ok", strerror(errno));
    if (ro != MAP_FAILED) munmap(ro, sizeof text);
    void* sh = mmap(NULL, sizeof text, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (sh == MAP_FAILED) {
        report("space", "file-map-shared-write", 0, "%s", strerror(errno));
    } else {
        ((char*)sh)[0] = 'M';
        msync(sh, sizeof text, MS_SYNC);
        char back[sizeof text] = {0};
        pread(fd, back, sizeof text, 0);
        report("space", "file-map-shared-write", back[0] == 'M', "read back '%c'", back[0]);
        munmap(sh, sizeof text);
    }
    void* ex = mmap(NULL, sizeof text, PROT_READ | PROT_EXEC, MAP_PRIVATE, fd, 0);
    report("space", "file-map-exec", ex != MAP_FAILED, ex == MAP_FAILED ? "%s" : "ok", strerror(errno));
    if (ex != MAP_FAILED) munmap(ex, sizeof text);
    close(fd);
}

// ── fs ──────────────────────────────────────────────────────────────────
static void probe_fs(void) {
    int d = open(g_dir, O_RDONLY | O_DIRECTORY);
    report("fs", "open-dir", d >= 0, "%s", d < 0 ? strerror(errno) : "ok");
    if (d < 0) return;
    int f = openat(d, "a.txt", O_RDWR | O_CREAT | O_TRUNC, 0644);
    report("fs", "openat-create", f >= 0, f < 0 ? "%s" : "ok", strerror(errno));
    if (f >= 0) {
        char buf[8] = {0};
        pwrite(f, "hello", 5, 0);
        ssize_t n = pread(f, buf, 5, 0);
        report("fs", "pread-pwrite", n == 5 && memcmp(buf, "hello", 5) == 0, "read %zd", n);
        report("fs", "ftruncate", ftruncate(f, 2) == 0, "%s", strerror(errno));
        int lk = flock(f, LOCK_EX | LOCK_NB);
        report("fs", "flock", lk == 0, lk ? "%s" : "ok", strerror(errno));
        struct flock fl = {.l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 1};
        int rl = fcntl(f, F_SETLK, &fl);
        report("fs", "fcntl-setlk", rl == 0, rl ? "%s" : "ok", strerror(errno));
        struct timespec ts[2] = {{1000000000, 0}, {1000000000, 0}};
        int ut = futimens(f, ts);
        report("fs", "futimens", ut == 0, ut ? "%s" : "ok", strerror(errno));
        close(f);
    }
    report("fs", "mkdirat", mkdirat(d, "sub", 0755) == 0, "%s", strerror(errno));
    int rn = renameat(d, "a.txt", d, "sub/b.txt");
    report("fs", "renameat", rn == 0, rn ? "%s" : "ok", strerror(errno));
    int f2 = openat(d, "c.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (f2 >= 0) close(f2);
    int ow = renameat(d, "c.txt", d, "sub/b.txt");   // over an existing file
    report("fs", "renameat-replace", ow == 0, ow ? "%s" : "ok", strerror(errno));
    int sl = symlinkat("sub/b.txt", d, "link");
    report("fs", "symlinkat", sl == 0, sl ? "%s" : "ok", strerror(errno));
    char target[256] = {0};
    ssize_t rl = readlinkat(d, "link", target, sizeof target - 1);
    report("fs", "readlinkat", rl > 0, rl > 0 ? "%s" : "%s", rl > 0 ? target : strerror(errno));
    int x = openat(d, "run.sh", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (x >= 0) close(x);
    int cm = fchmodat(d, "run.sh", 0755, 0);
    struct stat st;
    fstatat(d, "run.sh", &st, 0);
    report("fs", "set-executable", cm == 0 && (st.st_mode & S_IXUSR), "mode %o", (unsigned)st.st_mode & 0777);
    // A name outside ASCII, and a long path.
    int u = openat(d, "caf\xC3\xA9-\xE4\xB8\xAD.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    report("fs", "unicode-name", u >= 0, u < 0 ? "%s" : "ok", strerror(errno));
    if (u >= 0) close(u);
    char deep[1200];
    snprintf(deep, sizeof deep, "%s", "");
    int depth = 0, okdeep = 1;
    char rel[1200] = "";
    for (; depth < 12 && okdeep; ++depth) {
        strcat(rel, depth ? "/directory-with-a-long-name-0123456789" : "directory-with-a-long-name-0123456789");
        okdeep = mkdirat(d, rel, 0755) == 0;
    }
    snprintf(deep, sizeof deep, "%s/%s", g_dir, rel);
    report("fs", "long-path", okdeep, "%zu characters, %d levels%s%s", strlen(deep), depth, okdeep ? "" : ": ", okdeep ? "" : strerror(errno));
    DIR* dir = opendir(g_dir);
    int entries = 0;
    if (dir) { while (readdir(dir)) ++entries; closedir(dir); }
    report("fs", "readdir", entries > 0, "%d entries", entries);
    report("fs", "unlinkat", unlinkat(d, "link", 0) == 0, "%s", strerror(errno));
    close(d);
#ifdef __COSMOPOLITAN__
    if (IsWindows()) {
        // How a Windows path spelling reaches the file system.
        char win[4200];
        snprintf(win, sizeof win, "%s", g_dir);
        int w1 = open("C:/Windows/System32/drivers/etc/hosts", O_RDONLY);
        report("fs", "windows-path-forward-slash", w1 >= 0, w1 < 0 ? "%s" : "ok", strerror(errno));
        if (w1 >= 0) close(w1);
        int w2 = open("C:\\Windows\\System32\\drivers\\etc\\hosts", O_RDONLY);
        report("fs", "windows-path-backslash", w2 >= 0, w2 < 0 ? "%s" : "ok", strerror(errno));
        if (w2 >= 0) close(w2);
        int w3 = open("/c/Windows/System32/drivers/etc/hosts", O_RDONLY);
        report("fs", "windows-path-cosmo-style", w3 >= 0, w3 < 0 ? "%s" : "ok", strerror(errno));
        if (w3 >= 0) close(w3);
    }
#endif
}

// ── process ─────────────────────────────────────────────────────────────
static void probe_process(const char* self) {
    int out[2];
    if (pipe(out) != 0) { report("process", "spawn-pipe", 0, "pipe: %s", strerror(errno)); return; }
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, out[1], 1);
    posix_spawn_file_actions_addclose(&fa, out[0]);
    posix_spawn_file_actions_addchdir_np(&fa, g_dir);
    char* args[] = {(char*)self, (char*)"--child", (char*)"argument with spaces", (char*)"quote\"d", NULL};
    char* envs[] = {(char*)"PROBE_CHILD=1", (char*)"PATH=/usr/bin:/bin", NULL};
    pid_t pid;
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    int r = posix_spawn(&pid, self, &fa, NULL, args, envs);
    close(out[1]);
    posix_spawn_file_actions_destroy(&fa);
    if (r != 0) { report("process", "spawn", 0, "%s", strerror(r)); close(out[0]); return; }
    char buf[4096] = {0};
    size_t got = 0;
    ssize_t n;
    while ((n = read(out[0], buf + got, sizeof buf - 1 - got)) > 0) got += (size_t)n;
    close(out[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ms = (t1.tv_sec - t0.tv_sec) * 1e3 + (t1.tv_nsec - t0.tv_nsec) / 1e6;
    report("process", "spawn-wait-exit", WIFEXITED(status) && WEXITSTATUS(status) == 7, "status %d, %.1f ms", status, ms);
    report("process", "spawn-args", strstr(buf, "arg2=[argument with spaces]") && strstr(buf, "arg3=[quote\"d]"), "%s", buf);
    report("process", "spawn-env", strstr(buf, "env=1") != NULL, "child env seen: %s", strstr(buf, "env=1") ? "yes" : "no");
    report("process", "spawn-chdir", strstr(buf, "cwd-matches=1") != NULL, "child cwd as asked: %s", strstr(buf, "cwd-matches=1") ? "yes" : "no");
    // Terminate a running child.
    char* sleeper[] = {(char*)self, (char*)"--sleep", NULL};
    pid_t sp;
    if (posix_spawn(&sp, self, NULL, NULL, sleeper, environ) == 0) {
        usleep(200000);
        kill(sp, SIGTERM);
        int s2 = 0;
        waitpid(sp, &s2, 0);
        report("process", "terminate", WIFSIGNALED(s2) || (WIFEXITED(s2) && WEXITSTATUS(s2) != 0), "status %d", s2);
    } else {
        report("process", "terminate", 0, "spawn failed");
    }
    // A process group (openkal's job).
    pid_t gp;
    posix_spawnattr_t at;
    posix_spawnattr_init(&at);
    posix_spawnattr_setflags(&at, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&at, 0);
    int gr = posix_spawn(&gp, self, NULL, &at, sleeper, environ);
    if (gr == 0) {
        usleep(200000);
        int kg = kill(-gp, SIGTERM);
        int s3 = 0;
        waitpid(gp, &s3, 0);
        report("process", "process-group-terminate", kg == 0, kg ? "%s" : "status %d", kg ? strerror(errno) : "", s3);
    } else {
        report("process", "process-group-terminate", 0, "spawn: %s", strerror(gr));
    }
    posix_spawnattr_destroy(&at);
}

// ── task ────────────────────────────────────────────────────────────────
static void* task_body(void* a) { *(int*)a += 1; return a; }
static void probe_task(void) {
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);
    report("task", "parallelism", cpus > 0, "%ld", cpus);
    enum { N = 16 };
    pthread_t t[N];
    int v[N] = {0}, okc = 1;
    for (int i = 0; i < N; ++i) okc &= pthread_create(&t[i], NULL, task_body, &v[i]) == 0;
    for (int i = 0; i < N; ++i) pthread_join(t[i], NULL);
    int sum = 0;
    for (int i = 0; i < N; ++i) sum += v[i];
    report("task", "threads", okc && sum == N, "%d of %d", sum, N);
    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setstacksize(&a, 8u << 20);
    pthread_t big;
    int bv = 0;
    int bc = pthread_create(&big, &a, task_body, &bv);
    if (bc == 0) pthread_join(big, NULL);
    report("task", "sized-stack", bc == 0 && bv == 1, "%s", bc ? strerror(bc) : "8 MiB");
    pthread_attr_destroy(&a);
}

// ── net, datagram ───────────────────────────────────────────────────────
static void probe_net(void) {
    int l = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in sa = {.sin_family = AF_INET, .sin_port = 0};
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int okb = l >= 0 && bind(l, (struct sockaddr*)&sa, sizeof sa) == 0 && listen(l, 4) == 0;
    socklen_t sl = sizeof sa;
    getsockname(l, (struct sockaddr*)&sa, &sl);
    int c = socket(AF_INET, SOCK_STREAM, 0);
    int okc = okb && connect(c, (struct sockaddr*)&sa, sizeof sa) == 0;
    int s = okc ? accept(l, NULL, NULL) : -1;
    char buf[8] = {0};
    int okx = s >= 0 && send(c, "ping", 4, 0) == 4 && recv(s, buf, 4, 0) == 4 && memcmp(buf, "ping", 4) == 0;
    report("net", "tcp-loopback", okx, okx ? "port %d" : "%s", okx ? ntohs(sa.sin_port) : 0, strerror(errno));
    if (s >= 0) close(s);
    if (c >= 0) close(c);
    if (l >= 0) close(l);
    int l6 = socket(AF_INET6, SOCK_STREAM, 0);
    struct sockaddr_in6 s6 = {.sin6_family = AF_INET6, .sin6_addr = IN6ADDR_LOOPBACK_INIT};
    int ok6 = l6 >= 0 && bind(l6, (struct sockaddr*)&s6, sizeof s6) == 0;
    report("net", "tcp6-bind", ok6, ok6 ? "ok" : "%s", strerror(errno));
    if (l6 >= 0) close(l6);
    struct addrinfo hints = {.ai_socktype = SOCK_STREAM}, *res = NULL;
    int ga = getaddrinfo("localhost", "80", &hints, &res);
    report("net", "getaddrinfo-localhost", ga == 0, ga ? "%s" : "ok", ga ? gai_strerror(ga) : "");
    if (res) freeaddrinfo(res);
    int u1 = socket(AF_INET, SOCK_DGRAM, 0), u2 = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in ua = {.sin_family = AF_INET};
    ua.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(u1, (struct sockaddr*)&ua, sizeof ua);
    socklen_t ul = sizeof ua;
    getsockname(u1, (struct sockaddr*)&ua, &ul);
    sendto(u2, "dgram", 5, 0, (struct sockaddr*)&ua, sizeof ua);
    char ub[8] = {0};
    ssize_t ur = recvfrom(u1, ub, sizeof ub, 0, NULL, NULL);
    report("datagram", "udp-loopback", ur == 5 && memcmp(ub, "dgram", 5) == 0, "received %zd", ur);
    close(u1);
    close(u2);
}

// ── time, random, terminal, signal ──────────────────────────────────────
static volatile sig_atomic_t g_sig = 0;
static void on_sig(int s) { g_sig = s; }
static void probe_misc(void) {
    struct timespec m, w, res;
    int okm = clock_gettime(CLOCK_MONOTONIC, &m) == 0 && clock_getres(CLOCK_MONOTONIC, &res) == 0;
    report("time", "monotonic", okm, "resolution %ld ns", res.tv_nsec);
    int okw = clock_gettime(CLOCK_REALTIME, &w) == 0;
    report("time", "wall", okw && w.tv_sec > 1700000000, "%lld", (long long)w.tv_sec);
    struct timespec s0, s1, nap = {0, 50000000};
    clock_gettime(CLOCK_MONOTONIC, &s0);
    nanosleep(&nap, NULL);
    clock_gettime(CLOCK_MONOTONIC, &s1);
    double slept = (s1.tv_sec - s0.tv_sec) * 1e3 + (s1.tv_nsec - s0.tv_nsec) / 1e6;
    report("time", "sleep-50ms", slept >= 45 && slept < 500, "%.1f ms", slept);
    unsigned char rnd[32];
    report("random", "getentropy", getentropy(rnd, sizeof rnd) == 0, "%s", strerror(errno));
    int tty = isatty(1);
    struct termios tio;
    struct winsize ws;
    report("terminal", "isatty-stdout", 1, "%d", tty);
    report("terminal", "tcgetattr", tty ? tcgetattr(1, &tio) == 0 : 1, tty ? "%s" : "not a terminal here", strerror(errno));
    report("terminal", "window-size", tty ? ioctl(1, TIOCGWINSZ, &ws) == 0 : 1, tty ? "%dx%d" : "not a terminal here", ws.ws_col, ws.ws_row);
    struct sigaction sa = {0};
    sa.sa_handler = on_sig;
    sigaction(SIGTERM, &sa, NULL);
    raise(SIGTERM);
    report("process", "stop-request-signal", g_sig == SIGTERM, "handler saw %d", (int)g_sig);
}

static int child_main(int argc, char** argv) {
    char cwd[4096] = {0};
    getcwd(cwd, sizeof cwd);
    const char* e = getenv("PROBE_CHILD");
    printf("arg2=[%s] arg3=[%s] env=%s cwd=%s cwd-matches=%d\n", argc > 2 ? argv[2] : "", argc > 3 ? argv[3] : "",
           e ? e : "", cwd, strstr(cwd, "probe-scratch") != NULL);
    return 7;
}

int main(int argc, char** argv) {
    if (argc > 1 && strcmp(argv[1], "--child") == 0) return child_main(argc, argv);
    if (argc > 1 && strcmp(argv[1], "--sleep") == 0) { sleep(30); return 0; }
    if (argc > 1 && strcmp(argv[1], "--hello") == 0) { puts("hello"); return 0; }
    // The program by an absolute path: a spawn that changes directory resolves
    // a relative one after the change.
    static char self[4096];
    if (!realpath(argv[0], self)) snprintf(self, sizeof self, "%s", argv[0]);
    const char* tmp = getenv("RUNNER_TEMP");
    if (!tmp || !*tmp) tmp = getenv("TMPDIR");
    if (!tmp || !*tmp) tmp = "/tmp";
    snprintf(g_dir, sizeof g_dir, "%s/probe-scratch-%d", tmp, (int)getpid());
    mkdir(g_dir, 0755);
    probe_env(argc, argv);
    probe_exec();
    probe_space();
    probe_fs();
    probe_process(self);
    probe_task();
    probe_net();
    probe_misc();
    printf("{\"summary\":{\"ok\":%d,\"fail\":%d}}\n", g_ok, g_fail);
    return 0;
}
