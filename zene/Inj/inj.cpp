
// ================================================================================
// t.me/imguisource_bot | t.me/ImGui_Source_FIXNEY (ImGui Source) | t.me/il2cpp_src (shit src 🧨)
// ================================================================================

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cerrno>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <elf.h>
#include <asm/ptrace.h>
#include <dlfcn.h>
#include <link.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define LOG(fmt, ...) do { printf("[inj] " fmt "\n", ##__VA_ARGS__); fflush(stdout); } while(0)

static const char* DEFAULT_PKG = "com.Chillow.CustomRise";
static const char* REMOTE_SO   = "freecamk.so";

// ---------------- process helpers ----------------

static int find_pid_by_pkg(const char* pkg) {
    DIR* d = opendir("/proc");
    if (!d) return -1;
    int found = -1;
    struct dirent* de;
    while ((de = readdir(d))) {
        if (de->d_name[0] < '0' || de->d_name[0] > '9') continue;
        char path[64];
        snprintf(path, sizeof(path), "/proc/%s/cmdline", de->d_name);
        FILE* f = fopen(path, "r");
        if (!f) continue;
        char buf[256] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
        if (n == 0) continue;
        if (strcmp(buf, pkg) == 0) { found = atoi(de->d_name); break; }
    }
    closedir(d);
    return found;
}

static bool wait_gone(int pid, int ms) {
    for (int i = 0; i < ms / 100; i++) {
        char p[32];
        snprintf(p, sizeof(p), "/proc/%d", pid);
        if (access(p, F_OK) != 0) return true;
        usleep(100000);
    }
    return false;
}

static int wait_appear(const char* pkg, int ms) {
    for (int i = 0; i < ms / 200; i++) {
        int pid = find_pid_by_pkg(pkg);
        if (pid > 0) return pid;
        usleep(200000);
    }
    return -1;
}

static bool launch_pkg(const char* pkg) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
        "monkey -p %s -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1", pkg);
    int r = system(cmd);
    LOG("launch: monkey exit=%d", r);
    return r == 0;
}

static int proc_status_field(int pid, const char* key) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE* f = fopen(path, "r");
    if (!f) return -1;
    char line[256];
    int v = -1;
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen) == 0) {
            sscanf(line + klen, "%d", &v);
            break;
        }
    }
    fclose(f);
    return v;
}

// ---------------- maps / symbols ----------------

static uint64_t module_base(int pid, const char* name) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    char line[1024];
    uint64_t result = 0;
    size_t nlen = strlen(name);
    while (fgets(line, sizeof(line), f)) {
        unsigned long long start, off;
        char perms[8];
        if (sscanf(line, "%llx-%*x %7s %llx", &start, perms, &off) != 3) continue;
        if (off != 0) continue;
        char* p = strchr(line, '/');
        if (!p) continue;
        size_t plen = strlen(p);
        while (plen && (p[plen-1] == '\n' || p[plen-1] == '\r')) plen--;
        if (plen < nlen) continue;
        if (strncmp(p + plen - nlen, name, nlen) != 0) continue;
        if (plen > nlen && p[plen - nlen - 1] != '/') continue;
        result = start; break;
    }
    fclose(f);
    return result;
}

static uint64_t resolve_one(int pid, const char* sym) {
    void* s = dlsym(RTLD_DEFAULT, sym);
    if (!s) return 0;
    Dl_info info;
    if (!dladdr(s, &info) || !info.dli_fname) { LOG("%s: dladdr failed", sym); return 0; }
    const char* base = strrchr(info.dli_fname, '/');
    base = base ? base + 1 : info.dli_fname;
    uint64_t off = (uint64_t)s - (uint64_t)info.dli_fbase;
    uint64_t rb  = module_base(pid, base);
    LOG("%s: provider=%s off=0x%llx target_base=0x%llx", sym, base,
        (unsigned long long)off, (unsigned long long)rb);
    if (!rb) return 0;
    return rb + off;
}

static uint64_t resolve_dlopen(int pid) {
    uint64_t a = resolve_one(pid, "__loader_dlopen");
    if (a) { LOG("using __loader_dlopen -> 0x%llx", (unsigned long long)a); return a; }
    a = resolve_one(pid, "android_dlopen_ext");
    if (a) { LOG("using android_dlopen_ext -> 0x%llx", (unsigned long long)a); return a; }
    a = resolve_one(pid, "dlopen");
    if (a) { LOG("using dlopen -> 0x%llx", (unsigned long long)a); return a; }
    return 0;
}

// ---------------- ptrace helpers ----------------

static bool get_regs(int pid, struct user_pt_regs* regs) {
    struct iovec io = { regs, sizeof(*regs) };
    return ptrace(PTRACE_GETREGSET, pid, (void*)NT_PRSTATUS, &io) == 0;
}

static bool set_regs(int pid, const struct user_pt_regs* regs) {
    struct iovec io = { (void*)regs, sizeof(*regs) };
    return ptrace(PTRACE_SETREGSET, pid, (void*)NT_PRSTATUS, &io) == 0;
}

static bool rd_word(int pid, uint64_t addr, uint64_t* out) {
    errno = 0;
    long v = ptrace(PTRACE_PEEKDATA, pid, (void*)addr, 0);
    if (v == -1 && errno) return false;
    *out = (uint64_t)v;
    return true;
}

static bool wr_word(int pid, uint64_t addr, uint64_t val) {
    return ptrace(PTRACE_POKEDATA, pid, (void*)addr, (void*)val) == 0;
}

static bool wr_buf(int pid, uint64_t addr, const void* buf, size_t len) {
    const uint8_t* p = (const uint8_t*)buf;
    for (size_t off = 0; off < len; off += 8) {
        uint64_t word = 0;
        size_t chunk = (len - off) < 8 ? (len - off) : 8;
        memcpy(&word, p + off, chunk);
        if (!wr_word(pid, addr + off, word)) return false;
    }
    return true;
}

static bool rd_str(int pid, uint64_t addr, char* out, size_t max) {
    if (max == 0) return false;
    size_t i = 0;
    while (i + 1 < max) {
        uint64_t aligned = addr & ~7ULL;
        uint64_t w;
        if (!rd_word(pid, aligned, &w)) return false;
        const uint8_t* b = (const uint8_t*)&w;
        for (size_t k = addr - aligned; k < 8; k++) {
            if (i + 1 >= max) break;
            out[i] = (char)b[k];
            if (out[i] == 0) return true;
            i++;
            addr++;
        }
    }
    out[max - 1] = 0;
    return true;
}

static bool stack_lo(int pid, uint64_t* lo) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    FILE* f = fopen(path, "r");
    if (!f) return false;
    char line[1024];
    bool found = false;
    while (fgets(line, sizeof(line), f)) {
        if (!strstr(line, "[stack]")) continue;
        unsigned long long s;
        if (sscanf(line, "%llx-", &s) == 1) { *lo = s; found = true; break; }
    }
    fclose(f);
    return found;
}

static bool restore_and_fail(int pid, const struct user_pt_regs* saved, uint64_t saved_insn) {
    wr_word(pid, saved->pc, saved_insn);
    set_regs(pid, saved);
    return false;
}

static bool remote_call(int pid, uint64_t func, uint64_t* args, int nargs, uint64_t* result) {
    struct user_pt_regs regs, saved;
    if (!get_regs(pid, &regs)) { LOG("get_regs: %s", strerror(errno)); return false; }
    saved = regs;

    uint64_t saved_insn;
    if (!rd_word(pid, saved.pc, &saved_insn)) { LOG("rd pc failed"); return false; }
    if (!wr_word(pid, saved.pc, 0xD4200000ULL)) { LOG("wr BRK: %s", strerror(errno)); return false; }

    regs.pc = func;
    regs.regs[30] = saved.pc;
    for (int i = 0; i < 8; i++) regs.regs[i] = (i < nargs) ? args[i] : 0;
    if (!set_regs(pid, &regs)) {
        LOG("set_regs: %s", strerror(errno));
        return restore_and_fail(pid, &saved, saved_insn);
    }

    if (ptrace(PTRACE_CONT, pid, 0, 0) < 0) {
        LOG("CONT: %s", strerror(errno));
        return restore_and_fail(pid, &saved, saved_insn);
    }

    int sig = 0;
    for (int guard = 0; guard < 32; guard++) {
        int status;
        if (waitpid(pid, &status, 0) < 0) {
            LOG("waitpid: %s", strerror(errno));
            return restore_and_fail(pid, &saved, saved_insn);
        }
        if (!WIFSTOPPED(status)) {
            LOG("not stopped, status=0x%x", status);
            return restore_and_fail(pid, &saved, saved_insn);
        }
        sig = WSTOPSIG(status);

        if (!get_regs(pid, &regs)) {
            LOG("get_regs in loop failed");
            return restore_and_fail(pid, &saved, saved_insn);
        }

        if (sig == SIGTRAP && regs.pc == saved.pc) {
            *result = regs.regs[0];
            wr_word(pid, saved.pc, saved_insn);
            set_regs(pid, &saved);
            return true;
        }

        if (sig == SIGSEGV || sig == SIGBUS || sig == SIGILL || sig == SIGABRT) {
            LOG("fatal sig=%d at pc=0x%llx (expected 0x%llx)",
                sig, (unsigned long long)regs.pc, (unsigned long long)saved.pc);
            return restore_and_fail(pid, &saved, saved_insn);
        }

        LOG("spurious stop sig=%d pc=0x%llx, passing through",
            sig, (unsigned long long)regs.pc);
        if (ptrace(PTRACE_CONT, pid, 0, sig) < 0) {
            LOG("CONT(pass): %s", strerror(errno));
            return restore_and_fail(pid, &saved, saved_insn);
        }
    }
    LOG("too many stops");
    return restore_and_fail(pid, &saved, saved_insn);
}

static void log_dlerror(int pid) {
    uint64_t derr = resolve_one(pid, "dlerror");
    if (!derr) { LOG("dlerror: not resolvable"); return; }
    uint64_t ep = 0;
    if (!remote_call(pid, derr, nullptr, 0, &ep)) { LOG("dlerror call failed"); return; }
    if (!ep) { LOG("dlerror: (no error)"); return; }
    char eb[512] = {0};
    if (rd_str(pid, ep, eb, sizeof(eb))) LOG("dlerror: %s", eb);
}

static bool copy_file(const char* src, const char* dst) {
    FILE* in = fopen(src, "rb");
    if (!in) { LOG("open %s: %s", src, strerror(errno)); return false; }
    FILE* out = fopen(dst, "wb");
    if (!out) { LOG("open %s: %s", dst, strerror(errno)); fclose(in); return false; }
    char buf[4096]; size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            LOG("write %s fail", dst); fclose(in); fclose(out); return false;
        }
    }
    fclose(in); fclose(out);
    return true;
}

static uint64_t do_dlopen(int pid, uint64_t dl_addr, uint64_t str_addr) {
    // 3 аргумента: filename, flags, extinfo/caller=null
    uint64_t args[3] = { str_addr, (uint64_t)RTLD_NOW, 0 };
    uint64_t h = 0;
    if (!remote_call(pid, dl_addr, args, 3, &h)) return 0;
    return h;
}

static bool inject(int pid, const char* so_path, const char* pkg) {
    if (ptrace(PTRACE_ATTACH, pid, 0, 0) < 0) {
        LOG("attach: %s (SELinux/hidepid?)", strerror(errno));
        return false;
    }
    int status;
    waitpid(pid, &status, 0);
    LOG("attached pid=%d", pid);

    bool ok = false;
    struct user_pt_regs regs;
    if (!get_regs(pid, &regs)) { LOG("get_regs: %s", strerror(errno)); goto detach; }
    LOG("sp=0x%llx pc=0x%llx", (unsigned long long)regs.sp, (unsigned long long)regs.pc);

    {
        uint64_t dl_addr = resolve_dlopen(pid);
        if (!dl_addr) { LOG("no dlopen-like symbol found"); goto detach; }

        uint64_t lo;
        if (!stack_lo(pid, &lo)) { LOG("no [stack] mapping"); goto detach; }
        uint64_t str_addr = regs.sp - 0x8000;
        if (str_addr < lo + 0x1000) { LOG("stack too small"); goto detach; }
        if (!wr_buf(pid, str_addr, so_path, strlen(so_path) + 1)) {
            LOG("wr path @0x%llx: %s", (unsigned long long)str_addr, strerror(errno));
            goto detach;
        }
        LOG("path @0x%llx = %s", (unsigned long long)str_addr, so_path);

        uint64_t h = do_dlopen(pid, dl_addr, str_addr);
        LOG("dlopen(\"%s\") -> 0x%llx", so_path, (unsigned long long)h);

        if (h == 0) {
            log_dlerror(pid);

            int uid = proc_status_field(pid, "Uid:");
            int gid = proc_status_field(pid, "Gid:");
            LOG("app uid=%d gid=%d", uid, gid);

            const char* bases[] = { "/data/data", "/data/user/0" };
            for (size_t b = 0; b < sizeof(bases)/sizeof(bases[0]); b++) {
                char dir[PATH_MAX], dst[PATH_MAX];
                snprintf(dir, sizeof(dir), "%s/%s", bases[b], pkg);
                snprintf(dst, sizeof(dst), "%s/%s/libfreecamk.so", bases[b], pkg);
                struct stat sd;
                if (stat(dir, &sd) != 0 || !S_ISDIR(sd.st_mode)) {
                    LOG("skip %s: no dir", dir); continue;
                }
                if (!copy_file(so_path, dst)) continue;
                if (uid > 0) chown(dst, uid, gid);
                chmod(dst, 0644);
                LOG("copied -> %s (uid=%d gid=%d)", dst, uid, gid);

                if (!wr_buf(pid, str_addr, dst, strlen(dst) + 1)) {
                    LOG("wr new path failed"); continue;
                }
                uint64_t h2 = do_dlopen(pid, dl_addr, str_addr);
                LOG("dlopen(\"%s\") -> 0x%llx", dst, (unsigned long long)h2);
                if (h2 != 0) { ok = true; break; }
                log_dlerror(pid);
            }
        } else {
            ok = true;
        }
    }

detach:
    ptrace(PTRACE_DETACH, pid, 0, 0);
    LOG("detached");
    return ok;
}

int main(int argc, char** argv) {
    const char* pkg = argc > 1 ? argv[1] : DEFAULT_PKG;
    LOG("pkg=%s", pkg);

    char self[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (n <= 0) { LOG("readlink: %s", strerror(errno)); return 1; }
    self[n] = 0;
    char* slash = strrchr(self, '/');
    if (!slash) { LOG("bad self path"); return 1; }
    *slash = 0;

    char so_path[PATH_MAX];
    snprintf(so_path, sizeof(so_path), "%s/%s", self, REMOTE_SO);
    struct stat st;
    if (stat(so_path, &st) != 0) { LOG("missing %s", so_path); return 1; }
    LOG("so=%s (%lld b)", so_path, (long long)st.st_size);

    int pid = find_pid_by_pkg(pkg);
    if (pid > 0) {
        LOG("pid %d running, killing", pid);
        kill(pid, SIGKILL);
        if (!wait_gone(pid, 5000)) { LOG("won't die"); return 2; }
        LOG("dead");
    } else {
        LOG("not running");
    }

    LOG("launching");
    launch_pkg(pkg);
    pid = wait_appear(pkg, 30000);
    if (pid <= 0) { LOG("never appeared"); return 3; }
    LOG("new pid=%d", pid);
    usleep(500000);

    if (!inject(pid, so_path, pkg)) { LOG("INJECT FAIL"); return 4; }
    LOG("INJECT OK");
    return 0;
}
