// Native experiments for an observable execution unit. This is not a new
// OpenKal API or a capability claim; it records the kernel facts an API needs.
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <sstream>
#include <algorithm>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/prctl.h>
#else
#include <libproc.h>
#include <sys/sysctl.h>
#include <uuid/uuid.h>
#endif
#endif

namespace fs = std::filesystem;
using namespace std::chrono_literals;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void report(const char* name, bool value) {
    std::cout << "{\"check\":\"" << name << "\",\"ok\":" << (value ? "true" : "false") << "}\n";
    std::cout.flush();
    require(value, name);
}
template<class Predicate> bool until(Predicate predicate, std::chrono::milliseconds bound = 5000ms) {
    const auto end = std::chrono::steady_clock::now() + bound;
    do { if (predicate()) return true; std::this_thread::sleep_for(5ms); }
    while (std::chrono::steady_clock::now() < end);
    return false;
}
std::string text(const fs::path& path) {
    std::ifstream input(path); return {std::istreambuf_iterator<char>(input), {}};
}
void put(const fs::path& path, std::string_view value) { std::ofstream out(path); out << value; out.flush(); require(bool(out), "write_control_record"); }
enum class observation { empty, occupied, unknown };

#ifdef _WIN32
struct handle {
    HANDLE value = nullptr;
    handle() = default;
    explicit handle(HANDLE h) : value(h) {}
    void reset() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); value = nullptr; }
    ~handle() { reset(); }
    handle(const handle&) = delete;
    handle& operator=(const handle&) = delete;
};
std::string quote(const std::string& value) { return '"' + value + '"'; }
std::string executable() {
    char path[32768]; const auto size = GetModuleFileNameA(nullptr, path, sizeof path);
    require(size > 0 && size < sizeof path, "self_executable"); return {path, size};
}
HANDLE start(const std::vector<std::string>& args) {
    std::string command = quote(executable());
    for (const auto& arg : args) command += " " + quote(arg);
    STARTUPINFOA startup{}; startup.cb = sizeof startup;
    PROCESS_INFORMATION child{};
    require(CreateProcessA(nullptr, command.data(), nullptr, nullptr, FALSE, 0,
                           nullptr, nullptr, &startup, &child), "start_native_process");
    CloseHandle(child.hThread); return child.hProcess;
}
FILETIME created(HANDLE process) {
    FILETIME start{}, end{}, kernel{}, user{};
    require(GetProcessTimes(process, &start, &end, &kernel, &user), "read_worker_birth");
    return start;
}
bool same_birth(FILETIME left, FILETIME right) {
    return left.dwLowDateTime == right.dwLowDateTime && left.dwHighDateTime == right.dwHighDateTime;
}
DWORD members(HANDLE job) {
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION usage{};
    require(QueryInformationJobObject(job, JobObjectBasicAccountingInformation,
                &usage, sizeof usage, nullptr), "query_job_members");
    return usage.ActiveProcesses;
}
observation observe(const std::string& name) {
    handle job(OpenJobObjectA(JOB_OBJECT_QUERY, FALSE, name.c_str()));
    if (!job.value) {
        return GetLastError() == ERROR_FILE_NOT_FOUND ? observation::empty : observation::unknown;
    }
    return members(job.value) == 0 ? observation::empty : observation::occupied;
}
int worker(const fs::path& root) {
    put(root / "worker.ready", std::to_string(GetCurrentProcessId()));
    require(until([&] { return fs::exists(root / "worker.release"); }, 20000ms), "worker_release");
    put(root / "late.write", "writer finished");
    return 0;
}
int controller(const fs::path& root, const std::string& name, bool bound) {
    handle job(CreateJobObjectA(nullptr, name.c_str()));
    require(job.value && GetLastError() != ERROR_ALREADY_EXISTS, "new_named_job");
    if (bound) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit{};
        limit.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        require(SetInformationJobObject(job.value, JobObjectExtendedLimitInformation,
                    &limit, sizeof limit), "set_bound_job");
    }
    require(AssignProcessToJobObject(job.value, GetCurrentProcess()), "enter_private_job");
    handle child(start({"worker", root.string()}));
    require(until([&] { return fs::exists(root / "worker.ready"); }), "worker_ready");
    put(root / "controller.ready", std::to_string(members(job.value)));
    if (until([&] { return fs::exists(root / "normal.release"); }, 20000ms)) {
        require(WaitForSingleObject(child.value, 5000) == WAIT_OBJECT_0, "normal_worker_exit");
        put(root / "owner.only", std::to_string(members(job.value)));
        require(until([&] { return fs::exists(root / "owner.release"); }, 20000ms), "normal_owner_release");
    }
    return 0;
}
void windows_case(const fs::path& root, bool bound, bool normal) {
    fs::create_directories(root);
    const auto suffix = std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64())
        + "-" + std::to_string(bound) + "-" + std::to_string(normal);
    const auto name = "Global\\openkal-unit-probe-" + suffix;
    handle owner(start({"controller", root.string(), name, bound ? "1" : "0"}));
    handle child;
    DWORD worker_id = 0;
    FILETIME worker_birth{};
    try {
        require(until([&] { return fs::exists(root / "controller.ready"); }), "controller_ready");
        report("private_job_contains_helper_and_writer", text(root / "controller.ready") == "2");
        const auto pid = static_cast<DWORD>(std::stoul(text(root / "worker.ready")));
        worker_id = pid;
        child.value = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        require(child.value != nullptr, "hold_worker_process_identity");
        worker_birth = created(child.value);
        if (!normal && !bound) child.reset();
        report("live_unit_is_not_empty", observe(name) == observation::occupied);
        if (normal) {
            put(root / "worker.release", "go"); put(root / "normal.release", "go");
            require(until([&] { return fs::exists(root / "owner.only"); }), "owner_only_ready");
            report("normal_completion_counts_helper_separately", text(root / "owner.only") == "1");
            report("helper_only_is_occupied_not_empty", observe(name) == observation::occupied);
            put(root / "owner.release", "go");
            require(WaitForSingleObject(owner.value, 5000) == WAIT_OBJECT_0, "normal_controller_exit");
        } else {
            require(TerminateProcess(owner.value, 137), "terminate_controller");
            require(WaitForSingleObject(owner.value, 5000) == WAIT_OBJECT_0, "controller_terminated");
            owner.reset();
            if (!bound) {
                // Query before opening even a process handle in this observer:
                // no retained controller/worker handle may mask name lifetime.
                report("named_job_survives_without_observer_process_handles", observe(name) == observation::occupied);
                child.value = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                require(child.value != nullptr && same_birth(created(child.value), worker_birth), "hold_orphan_process_identity");
                report("writer_survives_unbound_controller", WaitForSingleObject(child.value, 0) == WAIT_TIMEOUT);
                report("named_job_remains_observable_without_controller_handle", observe(name) == observation::occupied);
                put(root / "worker.release", "go");
                require(until([&] { return fs::exists(root / "late.write"); }), "late_writer_output");
            }
        }
        bool false_empty = false;
        require(until([&] {
            const auto state = observe(name);
            const bool ended = WaitForSingleObject(child.value, 0) == WAIT_OBJECT_0;
            if (state == observation::empty && !ended) false_empty = true;
            return ended && state == observation::empty;
        }), "whole_job_empty_after_writer_exit");
        report("missing_named_job_never_precedes_live_writer", !false_empty);
        report("complete_job_is_empty", observe(name) == observation::empty);
    } catch (...) {
        if (owner.value) TerminateProcess(owner.value, 137);
        if (!child.value && worker_id)
            child.value = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, worker_id);
        if (child.value && same_birth(created(child.value), worker_birth)) TerminateProcess(child.value, 137);
        throw;
    }
}
#else
std::uint64_t born(pid_t pid) {
#ifdef __linux__
    auto stat = text(fs::path("/proc") / std::to_string(pid) / "stat");
    const auto end = stat.rfind(')');
    if (end == std::string::npos) return 0;
    std::istringstream fields(stat.substr(end + 2)); std::string field;
    for (int i = 0; i <= 19; ++i) if (!(fields >> field)) return 0;
    return std::stoull(field);
#else
    proc_bsdinfo info{};
    if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &info, sizeof info) != sizeof info) return 0;
    return std::uint64_t(info.pbi_start_tvsec) * 1000000ull + info.pbi_start_tvusec;
#endif
}
std::string instance() {
#ifdef __linux__
    char ns[256]; const auto size = readlink("/proc/self/ns/pid", ns, sizeof ns);
    if (size <= 0) return {};
    return text("/proc/sys/kernel/random/boot_id") + std::string(ns, static_cast<std::size_t>(size));
#else
    char value[128]; std::size_t size = sizeof value;
    if (sysctlbyname("kern.bootsessionuuid", value, &size, nullptr, 0)) return {};
    return std::string(value, size > 0 && value[size - 1] == '\0' ? size - 1 : size);
#endif
}
struct reference { pid_t group; std::uint64_t owner_born; std::string origin; };
observation observe(const reference& ref) {
    if (ref.group <= 0 || ref.owner_born == 0 || ref.origin.empty() || ref.origin != instance())
        return observation::unknown;
    if (kill(-ref.group, 0) == -1) return errno == ESRCH ? observation::empty : observation::unknown;
    const auto current = born(ref.group);
    if (current && current != ref.owner_born) return observation::unknown;
    return observation::occupied;
}
void reap() { while (waitpid(-1, nullptr, WNOHANG) > 0) {} }
void wait_reap(pid_t pid) {
    while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {}
}
pid_t launch(const fs::path& root) {
    const auto owner = fork(); require(owner >= 0, "fork_controller");
    if (owner != 0) return owner;
    if (setpgid(0, 0)) _exit(90);
    const auto driver = fork(); if (driver < 0) _exit(91);
    if (driver == 0) {
#ifdef __linux__
        const auto parent = getppid();
        if (prctl(PR_SET_PDEATHSIG, SIGKILL) || getppid() != parent) _exit(92);
#endif
        const auto writer = fork(); if (writer < 0) _exit(93);
        if (writer == 0) {
            put(root / "writer.ready", std::to_string(getpid()));
            if (!until([&] { return fs::exists(root / "writer.release"); }, 20000ms)) _exit(94);
            put(root / "late.write", "writer finished"); _exit(0);
        }
        put(root / "driver.ready", std::to_string(getpid()));
        wait_reap(writer); _exit(0);
    }
    put(root / "controller.ready", std::to_string(getpid()));
    bool reaped = false;
    if (!until([&] {
        if (!reaped && waitpid(driver, nullptr, WNOHANG) == driver) {
            reaped = true; put(root / "owner.only", "ready");
        }
        return fs::exists(root / "owner.release");
    }, 20000ms)) _exit(95);
    if (!reaped) wait_reap(driver);
    _exit(0);
}
std::size_t members(pid_t group) {
#ifdef __linux__
    std::size_t result = 0;
    for (const auto& entry : fs::directory_iterator("/proc")) {
        const auto name = entry.path().filename().string();
        if (name.empty() || name[0] < '1' || name[0] > '9') continue;
        const auto row = text(entry.path() / "stat"); const auto end = row.rfind(')');
        if (end == std::string::npos) continue;
        std::istringstream fields(row.substr(end + 2)); std::string state; long parent = 0, id = 0;
        if (fields >> state >> parent >> id && id == group) ++result;
    }
    return result;
#else
    pid_t ids[2048]{};
    const int bytes = proc_listpgrppids(static_cast<std::uint32_t>(group), ids, sizeof ids);
    require(bytes > 0, "list_private_group_members");
    return static_cast<std::size_t>(bytes) / sizeof(pid_t);
#endif
}
void posix_case(const fs::path& root, bool normal) {
    fs::create_directories(root);
    const auto owner = launch(root);
    try {
        require(until([&] { return fs::exists(root / "writer.ready") && fs::exists(root / "driver.ready")
                              && fs::exists(root / "controller.ready"); }), "private_unit_ready");
        reference ref{owner, born(owner), instance()};
        const auto writer = static_cast<pid_t>(std::stoi(text(root / "writer.ready")));
        const auto driver = static_cast<pid_t>(std::stoi(text(root / "driver.ready")));
        report("all_writers_in_declared_private_unit", getpgid(owner) == owner && getpgid(driver) == owner && getpgid(writer) == owner);
        report("live_unit_is_not_empty", observe(ref) == observation::occupied);
        auto wrong = ref; wrong.origin += "different";
        report("foreign_instance_is_unknown", observe(wrong) == observation::unknown);
        if (normal) {
            put(root / "writer.release", "go");
            require(until([&] { return fs::exists(root / "late.write"); }), "normal_writer_finished");
            require(until([&] { return fs::exists(root / "owner.only"); }), "normal_owner_only_ready");
            report("normal_completion_counts_helper_separately", members(owner) == 1);
            report("live_helper_does_not_mean_empty", observe(ref) == observation::occupied);
            put(root / "owner.release", "go");
        } else {
            require(kill(owner, SIGKILL) == 0, "kill_controller"); wait_reap(owner);
            report("leader_death_is_not_unit_empty", observe(ref) == observation::occupied);
            put(root / "writer.release", "go");
            require(until([&] { return fs::exists(root / "late.write"); }), "orphan_writer_finished");
            report("writer_can_finish_after_controller_death", true);
        }
        if (normal) wait_reap(owner);
        require(until([&] { reap(); return observe(ref) == observation::empty; }), "unit_reaped_empty");
        report("whole_unit_empty_after_all_members_finish", true);
    } catch (...) {
        kill(-owner, SIGKILL); kill(owner, SIGKILL); wait_reap(owner); reap(); throw;
    }
}
#ifdef __linux__
void reuse_case() {
    require(getpid() == 1, "private_pid_namespace_required");
    const char* outside = std::getenv("PROBE_OUTER_PID_NAMESPACE");
    require(outside && instance().find(outside) == std::string::npos, "outer_namespace_must_differ");
    auto make = [] {
        const auto child = fork(); require(child >= 0, "fork_reuse_leader");
        if (child == 0) { if (setpgid(0, 0)) _exit(96); for (;;) pause(); }
        require(until([&] { return getpgid(child) == child; }), "reused_group_ready");
        return child;
    };
    const auto first = make(); reference old{first, born(first), instance()};
    require(kill(-first, SIGKILL) == 0, "end_old_group"); wait_reap(first);
    report("old_group_proven_empty", observe(old) == observation::empty);
    std::this_thread::sleep_for(30ms);
    put("/proc/sys/kernel/ns_last_pid", std::to_string(first - 1));
    const auto next = make();
    try {
        report("native_group_identifier_actually_reused", next == first);
        report("new_owner_has_different_birth", born(next) != old.owner_born);
        report("reused_reference_does_not_claim_quiet", observe(old) == observation::unknown);
        report("observation_never_terminates_unrelated_unit", kill(next, 0) == 0);
    } catch (...) { kill(-next, SIGKILL); wait_reap(next); throw; }
    kill(-next, SIGKILL); wait_reap(next);
    report("reference_can_be_empty_after_replacement_ends", observe(old) == observation::empty);
}
#endif
#endif

int main(int argc, char** argv) {
    try {
#ifdef _WIN32
        if (argc > 2 && std::string(argv[1]) == "worker") return worker(argv[2]);
        if (argc > 4 && std::string(argv[1]) == "controller") return controller(argv[2], argv[3], std::string(argv[4]) == "1");
#else
#ifdef __linux__
        if (argc > 1 && std::string(argv[1]) == "--pid-reuse") { reuse_case(); return 0; }
        require(prctl(PR_SET_CHILD_SUBREAPER, 1) == 0, "test_harness_reaps_orphans");
#endif
#endif
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto root = fs::temp_directory_path() / ("openkal-unit-probe-" + std::to_string(nonce));
        fs::create_directories(root);
        try {
#ifdef _WIN32
            windows_case(root / "normal", false, true);
            windows_case(root / "owner-lost", false, false);
            windows_case(root / "bound-owner-lost", true, false);
#else
            posix_case(root / "normal", true);
            posix_case(root / "owner-lost", false);
#endif
        } catch (...) { fs::remove_all(root); throw; }
        fs::remove_all(root);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "unit probe failed: " << error.what() << '\n'; return 1;
    }
}
