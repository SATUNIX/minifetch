// minifetch.c — pure C (C99) tiny "screenfetch-like" with layout switching + ASCII animation
// Build: gcc -O2 -pipe -s -Wall -Wextra -Wpedantic -Wno-unused-parameter -std=c99 -o minifetch minifetch.c
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <errno.h>

#include "config.h"
#include "logo_embedded.inc"

// ---------- Globals ----------
static volatile sig_atomic_t g_stop = 0;

// ---------- Small utilities ----------
static void die(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

static void install_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static bool term_is_tty(void) {
    return isatty(STDOUT_FILENO) == 1;
}

static void term_hide_cursor(bool on) {
    if (on) fputs("\x1b[?25l", stdout);
    else    fputs("\x1b[?25h", stdout);
    fflush(stdout);
}

static void term_alt_screen(bool on) {
    if (on) fputs("\x1b[?1049h", stdout);
    else    fputs("\x1b[?1049l", stdout);
    fflush(stdout);
}

static void term_move(int row, int col) {
    if (row < 1) row = 1;
    if (col < 1) col = 1;
    fprintf(stdout, "\x1b[%d;%dH", row, col);
}

static void term_clear_to_eol(void) {
    fputs("\x1b[K", stdout);
}

static void msleep_int(int ms) {
    if (ms <= 0) return;
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
        if (g_stop) break;
    }
}

static void rstrip(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == ' ' || s[n-1] == '\t')) {
        s[n-1] = '\0';
        n--;
    }
}

static void sanitize_ascii_printable(char *s, size_t allow_extra_len) {
    // Keep printable ASCII plus newline and ESC (for ANSI sequences), blank the rest.
    (void)allow_extra_len;
    for (char *p = s; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c >= 32 && c < 127) continue;
        if (c == '\n' || c == '\x1b') continue;
        *p = '?';
    }
}

static void copy_trunc(char *dst, size_t dstsz, const char *src) {
    if (!dst || dstsz == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t len = strlen(src);
    if (len >= dstsz) len = dstsz - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static bool path_is_exec(const char *p) {
    return (p && access(p, X_OK) == 0);
}

static bool command_exists_in_path(const char *name) {
    // Search PATH for "name"
    const char *path = getenv("PATH");
    if (!path || !*path) return false;
    size_t name_len = strlen(name);
    char buf[1024];
    const char *s = path;
    while (*s) {
        const char *colon = strchr(s, ':');
        size_t len = colon ? (size_t)(colon - s) : strlen(s);
        if (len > 0 && len < sizeof(buf)) {
            memcpy(buf, s, len);
            buf[len] = '\0';
            size_t need = len + 1 + name_len + 1;
            if (need < sizeof(buf)) {
                buf[len] = '/';
                memcpy(buf + len + 1, name, name_len + 1);
                if (path_is_exec(buf)) return true;
            }
        }
        if (!colon) break;
        s = colon + 1;
    }
    return false;
}

// Execute a constant command and return first line of stdout (trimmed). Returns empty string if none.
static void exec_read_first_line(const char *cmd, char *out, size_t outsz) {
    if (!cmd || !*cmd) { if (outsz) out[0] = '\0'; return; }
    FILE *fp = popen(cmd, "r");
    if (!fp) { if (outsz) out[0] = '\0'; return; }
    if (fgets(out, (int)outsz, fp) == NULL) { if (outsz) out[0] = '\0'; }
    pclose(fp);
    rstrip(out);
}

// Human-readable bytes (IEC)
static void human_bytes(unsigned long long bytes, char *out, size_t outsz) {
    const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    int ui = 0;
    double v = (double)bytes;
    while (v >= 1024.0 && ui < 5) { v /= 1024.0; ui++; }
    snprintf(out, outsz, "%.1f %s", v, units[ui]);
}

// ---------- Collectors ----------
static void get_distro(char *out, size_t outsz) {
    // Parse PRETTY_NAME from /etc/os-release or /usr/lib/os-release
    const char *candidates[] = {"/etc/os-release", "/usr/lib/os-release"};
    FILE *f = NULL;
    for (size_t i = 0; i < 2 && !f; ++i) f = fopen(candidates[i], "r");
    if (!f) { snprintf(out, outsz, "Linux"); return; }
    char line[512];
    char pretty[256] = {0};
    while (fgets(line, (int)sizeof(line), f)) {
        if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
            char *p = line + 12;
            rstrip(p);
            // Remove surrounding quotes if present
            if (p[0] == '"' || p[0] == '\'') {
                char q = p[0];
                p++;
                char *e = strrchr(p, q);
                if (e) *e = '\0';
            }
            copy_trunc(pretty, sizeof(pretty), p);
            break;
        }
    }
    fclose(f);
    if (pretty[0]) copy_trunc(out, outsz, pretty);
    else copy_trunc(out, outsz, "Linux");
}

static void get_host(char *out, size_t outsz) {
    FILE *f = fopen("/etc/hostname", "r");
    if (f) {
    if (fgets(out, (int)outsz, f)) { rstrip(out); fclose(f); return; }
        fclose(f);
    }
    struct utsname u;
    if (uname(&u) == 0) { copy_trunc(out, outsz, u.nodename); return; }
    copy_trunc(out, outsz, "unknown");
}

static void get_kernel(char *out, size_t outsz) {
    struct utsname u;
    if (uname(&u) == 0) { copy_trunc(out, outsz, u.release); return; }
    copy_trunc(out, outsz, "unknown");
}

static void get_uptime(char *out, size_t outsz) {
    FILE *f = fopen("/proc/uptime", "r");
    if (!f) { snprintf(out, outsz, "n/a"); return; }
    double up = 0.0;
    if (fscanf(f, "%lf", &up) != 1) { fclose(f); snprintf(out, outsz, "n/a"); return; }
    fclose(f);
    long mins = (long)(up / 60.0);
    long days = mins / (60 * 24);
    mins -= days * 60 * 24;
    long hours = mins / 60;
    mins -= hours * 60;
    if (days > 0) snprintf(out, outsz, "%ldd %ldh %ldm", days, hours, mins);
    else if (hours > 0) snprintf(out, outsz, "%ldh %ldm", hours, mins);
    else snprintf(out, outsz, "%ldm", mins);
}

static void get_packages(char *out, size_t outsz) {
    if (command_exists_in_path("pacman")) {
        exec_read_first_line("pacman -Qq | wc -l", out, outsz);
        if (out[0]) return;
    }
    if (command_exists_in_path("dpkg")) {
        exec_read_first_line("dpkg -l | grep -E '^ii|^hi' | wc -l", out, outsz);
        if (out[0]) return;
    }
    if (command_exists_in_path("dpkg-query")) {
        exec_read_first_line("dpkg-query -f '.\\n' -W | wc -l", out, outsz);
        if (out[0]) return;
    }
    snprintf(out, outsz, "n/a");
}

static void get_shell_name(char *out, size_t outsz) {
    const char *sh = getenv("SHELL");
    if (!sh || !*sh) { snprintf(out, outsz, "n/a"); return; }
    const char *base = strrchr(sh, '/');
    base = base ? (base + 1) : sh;
    char tmp[256];
    copy_trunc(tmp, sizeof(tmp), base);
    // whitelist sanitize to [A-Za-z0-9_.-]
    for (size_t i = 0; tmp[i]; ++i) {
        char c = tmp[i];
        if (!(isalnum((unsigned char)c) || c == '_' || c == '.' || c == '-')) {
            tmp[i] = '_';
        }
    }
    if (tmp[0]) copy_trunc(out, outsz, tmp);
    else copy_trunc(out, outsz, "unknown");
}

static void get_resolution(char *out, size_t outsz) {
    // Hyprland JSON first if env var set and hyprctl exists
    const char *hypr_env = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (hypr_env && command_exists_in_path("hyprctl")) {
        // Parse pairs of "width":N and "height":N from hyprctl -j monitors,
        // join as WxH, comma-separated.
        char buf[16384] = {0};
        FILE *fp = popen("hyprctl -j monitors", "r");
        if (fp) {
            size_t n = fread(buf, 1, sizeof(buf)-1, fp);
            buf[n] = '\0';
            pclose(fp);
            // naive parse
            int w = -1, h = -1;
            char outacc[1024] = {0};
            const char *p = buf;
            while (*p) {
                int val = -1;
                if (sscanf(p, "%*[^0-9]%d", &val) == 1) {
                    // We need to detect keys; do simple strstr windows from p
                    const char *kw = strstr(p, "\"width\"");
                    const char *kh = strstr(p, "\"height\"");
                    if (kw && kw < p + 32) {
                        // find actual width value near kw
                        const char *pp = kw;
                        int tmpw = -1;
                        while (*pp && *pp != '\n' && (pp - kw) < 80) {
                            if (sscanf(pp, "%*[^0-9]%d", &tmpw) == 1) { w = tmpw; break; }
                            pp++;
                        }
                    }
                    if (kh && kh < p + 32) {
                        const char *pp = kh;
                        int tmph = -1;
                        while (*pp && *pp != '\n' && (pp - kh) < 80) {
                            if (sscanf(pp, "%*[^0-9]%d", &tmph) == 1) { h = tmph; break; }
                            pp++;
                        }
                    }
                    if (w > 0 && h > 0) {
                        char one[64];
                        snprintf(one, sizeof(one), "%dx%d", w, h);
                        if (outacc[0]) strncat(outacc, ", ", sizeof(outacc)-strlen(outacc)-1);
                        strncat(outacc, one, sizeof(outacc)-strlen(outacc)-1);
                        w = h = -1;
                    }
                }
                p++;
            }
            if (outacc[0]) { copy_trunc(out, outsz, outacc); return; }
        }
    }
    // Wayland: wlr-randr
    if (command_exists_in_path("wlr-randr")) {
        char line[2048] = {0};
        exec_read_first_line("wlr-randr | grep -E '\\*' | awk '{print $2}' | paste -sd, -", line, sizeof(line));
        if (line[0]) { copy_trunc(out, outsz, line); return; }
    }
    // wayland-info (very rough)
    if (command_exists_in_path("wayland-info")) {
        char line[2048] = {0};
        exec_read_first_line("wayland-info 2>/dev/null | grep -Eo '[0-9]+x[0-9]+' | head -n1", line, sizeof(line));
        if (line[0]) { copy_trunc(out, outsz, line); return; }
    }
    // X11: xrandr star-marked modes
    if (command_exists_in_path("xrandr")) {
        char line[2048] = {0};
        exec_read_first_line("xrandr 2>/dev/null | grep '\\*' | awk '{print $1}' | paste -sd, -", line, sizeof(line));
        if (line[0]) { copy_trunc(out, outsz, line); return; }
    }
    snprintf(out, outsz, "n/a");
}

static void get_wm_de(char *wm_out, size_t wm_sz, char *de_out, size_t de_sz) {
    const char *wm = "n/a";
    if (getenv("HYPRLAND_INSTANCE_SIGNATURE")) wm = "Hyprland";
    else if (getenv("SWAYSOCK")) wm = "Sway";
    else if (getenv("XDG_CURRENT_DESKTOP")) wm = "X11/Wayland";
    copy_trunc(wm_out, wm_sz, wm);

    const char *xde = getenv("XDG_CURRENT_DESKTOP");
    if (!xde || !*xde) xde = getenv("DESKTOP_SESSION");
    if (!xde || !*xde) { snprintf(de_out, de_sz, "n/a"); return; }
    char tmp[256]; copy_trunc(tmp, sizeof(tmp), xde); sanitize_ascii_printable(tmp, 0);
    copy_trunc(de_out, de_sz, tmp);
}

static void get_cpu(char *out, size_t outsz) {
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) { snprintf(out, outsz, "n/a"); return; }
    char line[512];
    char model[256] = {0};
    int cores = 0;
    while (fgets(line, (int)sizeof(line), f)) {
        if (strncmp(line, "model name", 10) == 0) {
            const char *colon = strchr(line, ':');
            if (colon) {
                char *p = (char*)colon + 1;
                while (*p == ' ' || *p == '\t') p++;
                rstrip(p);
                if (!model[0]) copy_trunc(model, sizeof(model), p);
            }
        } else if (strncmp(line, "processor", 9) == 0) {
            cores++;
        }
    }
    fclose(f);
    if (!model[0]) snprintf(model, sizeof(model), "CPU");
    snprintf(out, outsz, "%s (%d)", model, (cores > 0 ? cores : 1));
}

static void get_gpu(char *out, size_t outsz) {
    if (!command_exists_in_path("lspci")) { snprintf(out, outsz, "n/a"); return; }
    char line[1024] = {0};
    exec_read_first_line("lspci | grep -E 'VGA|3D|Display' | sed 's/ (rev.*)//' | head -n1", line, sizeof(line));
    if (!line[0]) { snprintf(out, outsz, "n/a"); return; }
    copy_trunc(out, outsz, line);
}

static void get_mem(char *out, size_t outsz) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) { snprintf(out, outsz, "n/a"); return; }
    char line[256];
    unsigned long long mem_total_kb = 0, mem_avail_kb = 0;
    while (fgets(line, (int)sizeof(line), f)) {
        if (sscanf(line, "MemTotal: %llu kB", &mem_total_kb) == 1) continue;
        if (sscanf(line, "MemAvailable: %llu kB", &mem_avail_kb) == 1) continue;
    }
    fclose(f);
    if (mem_total_kb == 0) { snprintf(out, outsz, "n/a"); return; }
    unsigned long long used = (mem_total_kb - mem_avail_kb) * 1024ULL;
    unsigned long long total = mem_total_kb * 1024ULL;
    char u[64], t[64];
    human_bytes(used, u, sizeof(u));
    human_bytes(total, t, sizeof(t));
    snprintf(out, outsz, "%s / %s", u, t);
}

static void get_disk_root(char *out, size_t outsz) {
    struct statvfs s;
    if (statvfs("/", &s) != 0) { snprintf(out, outsz, "n/a"); return; }
    unsigned long long total = (unsigned long long)s.f_blocks * (unsigned long long)s.f_frsize;
    unsigned long long avail = (unsigned long long)s.f_bavail * (unsigned long long)s.f_frsize;
    unsigned long long used = total - avail;
    int pct = (total > 0) ? (int)((used * 100ULL) / total) : 0;
    char u[64], t[64];
    human_bytes(used, u, sizeof(u));
    human_bytes(total, t, sizeof(t));
    snprintf(out, outsz, "%s / %s (%d%%)", u, t, pct);
}

// ---------- Frames parsing ----------
typedef struct {
    char  *buf;        // owning buffer (copy from embedded)
    char **lines;      // array of pointers into buf
    int    line_count;
    int    max_w;
} FrameParsed;

typedef struct {
    FrameParsed *frames;
    unsigned int count;
    int logo_w_max;
    int logo_h_max;
} Frames;

static int line_visual_width(const char *s) {
    int w = 0;
    for (const char *p = s; *p; ++p) {
        if (*p == '\x1b') {
            // Skip ANSI escapes
            const char *q = p + 1;
            if (*q == '[') {
                q++;
                while (*q && !isalpha((unsigned char)*q)) q++;
                if (*q) { p = q; continue; }
            }
        }
        w++;
    }
    return w;
}

static void frames_free(Frames *F) {
    if (!F || !F->frames) return;
    for (unsigned int i = 0; i < F->count; ++i) {
        free(F->frames[i].lines);
        free(F->frames[i].buf);
    }
    free(F->frames);
    F->frames = NULL;
    F->count = 0;
    F->logo_w_max = 0;
    F->logo_h_max = 0;
}

static void split_lines_inplace(char *buf, char ***out_lines, int *out_count, int *out_maxw) {
    int lines_cap = 16;
    int lines_cnt = 0;
    char **lines = (char**)malloc((size_t)lines_cap * sizeof(char*));
    if (!lines) die("OOM");

    int maxw = 0;
    char *p = buf;
    lines[lines_cnt++] = p;
    while (*p) {
        if (*p == '\r') *p = '\0';
        if (*p == '\n') {
            *p = '\0';
            if (*(p+1)) {
                if (lines_cnt == lines_cap) {
                    lines_cap *= 2;
                    char **tmp = (char**)realloc(lines, (size_t)lines_cap * sizeof(char*));
                    if (!tmp) { free(lines); die("OOM"); }
                    lines = tmp;
                }
                lines[lines_cnt++] = p + 1;
            }
        }
        p++;
    }
    for (int i = 0; i < lines_cnt; ++i) {
        int w = line_visual_width(lines[i]);
        if (w > maxw) maxw = w;
    }
    *out_lines = lines;
    *out_count = lines_cnt;
    *out_maxw  = maxw;
}

static Frames frames_parse_from_embed(void) {
    Frames F; memset(&F, 0, sizeof(F));
    unsigned int n = EMBED_FRAME_COUNT;
    if (n == 0) {
        // Fallback 1-line
        const char *fallback = "minifetch";
        F.frames = (FrameParsed*)calloc(1, sizeof(FrameParsed));
        if (!F.frames) die("OOM");
        F.count = 1;
        F.frames[0].buf = strdup(fallback);
        F.frames[0].lines = (char**)malloc(sizeof(char*));
        if (!F.frames[0].buf || !F.frames[0].lines) die("OOM");
        F.frames[0].lines[0] = F.frames[0].buf;
        F.frames[0].line_count = 1;
        F.frames[0].max_w = (int)strlen(fallback);
        F.logo_w_max = F.frames[0].max_w;
        F.logo_h_max = 1;
        return F;
    }
    F.frames = (FrameParsed*)calloc(n, sizeof(FrameParsed));
    if (!F.frames) die("OOM");
    F.count = n;
    int wmax = 0, hmax = 0;
    for (unsigned int i = 0; i < n; ++i) {
        const unsigned char *data = EMBED_FRAMES[i].data;
        unsigned int len = EMBED_FRAMES[i].len;
        char *buf = (char*)malloc(len + 1U);
        if (!buf) die("OOM");
        memcpy(buf, data, len);
        buf[len] = '\0';
        // Ensure buffer is printable-friendly
        sanitize_ascii_printable(buf, 0);
        char **lines = NULL; int lc = 0; int mw = 0;
        split_lines_inplace(buf, &lines, &lc, &mw);
        F.frames[i].buf = buf;
        F.frames[i].lines = lines;
        F.frames[i].line_count = lc;
        F.frames[i].max_w = mw;
        if (mw > wmax) wmax = mw;
        if (lc > hmax) hmax = lc;
    }
    F.logo_w_max = wmax;
    F.logo_h_max = hmax;
    return F;
}

// ---------- Info building ----------
typedef struct {
    char **lines;
    int    count;
} InfoLines;

static void info_free(InfoLines *I) {
    if (!I || !I->lines) return;
    for (int i = 0; i < I->count; ++i) free(I->lines[i]);
    free(I->lines);
    I->lines = NULL; I->count = 0;
}

static void add_line(InfoLines *I, const char *label, const char *value) {
    size_t need = strlen(CFG_LABEL_COLOR) + strlen(label) + strlen("\x1b[0m ") +
                  strlen(CFG_VALUE_COLOR) + strlen(value) + strlen("\x1b[0m") + 8;
    char *s = (char*)malloc(need);
    if (!s) die("OOM");
    snprintf(s, need, "%s%s\x1b[0m %s%s\x1b[0m", CFG_LABEL_COLOR, label, CFG_VALUE_COLOR, value);
    int newc = I->count + 1;
    char **tmp = (char**)realloc(I->lines, (size_t)newc * sizeof(char*));
    if (!tmp) { free(s); die("OOM"); }
    I->lines = tmp;
    I->lines[I->count] = s;
    I->count = newc;
}

static InfoLines build_info_lines(void) {
    InfoLines I; I.lines = NULL; I.count = 0;

    char os[256]={0}, host[256]={0}, kern[128]={0}, upt[64]={0}, pkgs[64]={0};
    char sh[128]={0}, res[512]={0}, wm[64]={0}, de[256]={0};
    char cpu[512]={0}, gpu[512]={0}, mem[160]={0}, disk[160]={0};

    if (CFG_SHOW_DISTRO) { get_distro(os, sizeof(os)); add_line(&I, "OS:", os); }
    if (CFG_SHOW_HOST)   { get_host(host, sizeof(host)); add_line(&I, "Host:", host); }
    if (CFG_SHOW_KERNEL) { get_kernel(kern, sizeof(kern)); add_line(&I, "Kernel:", kern); }
    if (CFG_SHOW_UPTIME) { get_uptime(upt, sizeof(upt)); add_line(&I, "Uptime:", upt); }
    if (CFG_SHOW_PKGS)   { get_packages(pkgs, sizeof(pkgs)); add_line(&I, "Packages:", pkgs); }
    if (CFG_SHOW_SHELL)  { get_shell_name(sh, sizeof(sh)); add_line(&I, "Shell:", sh); }
    if (CFG_SHOW_RES)    { get_resolution(res, sizeof(res)); add_line(&I, "Resolution:", res); }
    if (CFG_SHOW_WM || CFG_SHOW_DE) {
        get_wm_de(wm, sizeof(wm), de, sizeof(de));
        if (CFG_SHOW_WM) add_line(&I, "WM:", wm);
        if (CFG_SHOW_DE) add_line(&I, "DE:", de);
    }
    if (CFG_SHOW_CPU)    { get_cpu(cpu, sizeof(cpu)); add_line(&I, "CPU:", cpu); }
    if (CFG_SHOW_GPU)    { get_gpu(gpu, sizeof(gpu)); add_line(&I, "GPU:", gpu); }
    if (CFG_SHOW_MEM)    { get_mem(mem, sizeof(mem)); add_line(&I, "Memory:", mem); }
    if (CFG_SHOW_DISK)   { get_disk_root(disk, sizeof(disk)); add_line(&I, "Disk:", disk); }

    return I;
}

// ---------- Rendering helpers ----------
static void print_spaces(int n) {
    for (int i = 0; i < n; ++i) fputc(' ', stdout);
}

static void print_padded_logo_line(const char *s, int pad_to) {
    // Print s and pad with spaces to pad_to width
    int len = line_visual_width(s);
    fputs(CFG_LOGO_COLOR, stdout);
    fputs(s, stdout);
    int pad = pad_to - len;
    for (int i = 0; i < pad; ++i) fputc(' ', stdout);
    fputs("\x1b[0m", stdout);
}

// ---------- Renderers ----------
static void render_static_side(const Frames *F, const InfoLines *I, int gap) {
    const FrameParsed *f0 = &F->frames[0];
    int rows = (F->logo_h_max > I->count) ? F->logo_h_max : I->count;
    for (int r = 0; r < rows; ++r) {
        const char *left = (r < f0->line_count) ? f0->lines[r] : "";
        print_padded_logo_line(left, F->logo_w_max);
        print_spaces(gap);
        if (r < I->count) fputs(I->lines[r], stdout);
        fputc('\n', stdout);
    }
    fflush(stdout);
}

static void render_static_top(const Frames *F, const InfoLines *I) {
    const FrameParsed *f0 = &F->frames[0];
    for (int r = 0; r < f0->line_count; ++r) {
        print_padded_logo_line(f0->lines[r], F->logo_w_max);
        fputc('\n', stdout);
    }
    for (int r = 0; r < I->count; ++r) {
        fputs(I->lines[r], stdout);
        fputc('\n', stdout);
    }
    fflush(stdout);
}

typedef struct {
    int *seq;
    int len;
} FrameSeq;

static FrameSeq build_sequence(unsigned int n, int mode) {
    FrameSeq S; S.seq = NULL; S.len = 0;
    if (n == 0) return S;
    if (mode == CFG_ANIM_LOOP || mode == CFG_ANIM_ONCE) {
        S.len = (int)n;
        S.seq = (int*)malloc((size_t)S.len * sizeof(int));
        if (!S.seq) die("OOM");
        for (unsigned int i = 0; i < n; ++i) S.seq[i] = (int)i;
    } else if (mode == CFG_ANIM_PINGPONG) {
        if (n == 1) {
            S.len = 1;
            S.seq = (int*)malloc(sizeof(int));
            if (!S.seq) die("OOM");
            S.seq[0] = 0;
        } else {
            S.len = (int)(n + (n - 2)); // 0..n-1..1
            S.seq = (int*)malloc((size_t)S.len * sizeof(int));
            if (!S.seq) die("OOM");
            int k = 0;
            for (unsigned int i = 0; i < n; ++i) S.seq[k++] = (int)i;
            for (int i = (int)n - 2; i >= 1; --i) S.seq[k++] = i;
        }
    }
    return S;
}

static void free_sequence(FrameSeq *S) {
    free(S->seq);
    S->seq = NULL; S->len = 0;
}

static void animated_render_side(const Frames *F, const InfoLines *I, int fps, double duration, int mode) {
    const int gap = CFG_GAP_SPACES;
    int rows = (F->logo_h_max > I->count) ? F->logo_h_max : I->count;

    if (CFG_USE_ALT_SCREEN) term_alt_screen(true);
    if (CFG_HIDE_CURSOR_DURING_ANIM) term_hide_cursor(true);

    // Print the info column once at col = logo_w_max + gap + 1
    for (int r = 0; r < rows; ++r) {
        term_move(r + 1, F->logo_w_max + gap + 1);
        if (r < I->count) fputs(I->lines[r], stdout);
        term_clear_to_eol();
    }
    fflush(stdout);

    FrameSeq S = build_sequence(F->count, mode);
    if (S.len == 0) { /* nothing */ }

    const int frame_ms = 1000 / (fps <= 0 ? 1 : fps);
    double elapsed = 0.0;
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    bool running = true;
    while (running && !g_stop) {
        for (int i = 0; i < S.len && !g_stop; ++i) {
            int fi = S.seq ? S.seq[i] : 0;
            for (int r = 0; r < F->logo_h_max; ++r) {
                term_move(r + 1, 1);
                const char *ln = (r < F->frames[fi].line_count) ? F->frames[fi].lines[r] : "";
                print_padded_logo_line(ln, F->logo_w_max);
                term_clear_to_eol();
            }
            fflush(stdout);
            msleep_int(frame_ms);

            if (duration > 0.0) {
                struct timespec t1;
                clock_gettime(CLOCK_MONOTONIC, &t1);
                elapsed = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec)/1e9;
                if (elapsed >= duration) { running = false; break; }
            }
        }
        if (mode == CFG_ANIM_ONCE) break;
        if (S.len == 0) break;
    }

    if (CFG_HIDE_CURSOR_DURING_ANIM) term_hide_cursor(false);
    if (CFG_USE_ALT_SCREEN) { term_alt_screen(false); fputc('\n', stdout); }
    free_sequence(&S);
}

static void animated_render_top(const Frames *F, const InfoLines *I, int fps, double duration, int mode) {
    if (CFG_USE_ALT_SCREEN) term_alt_screen(true);
    if (CFG_HIDE_CURSOR_DURING_ANIM) term_hide_cursor(true);

    // Print info once starting below logo area
    for (int r = 0; r < I->count; ++r) {
        term_move(F->logo_h_max + r + 1, 1);
        fputs(I->lines[r], stdout);
        term_clear_to_eol();
    }
    fflush(stdout);

    FrameSeq S = build_sequence(F->count, mode);
    const int frame_ms = 1000 / (fps <= 0 ? 1 : fps);
    double elapsed = 0.0;
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    bool running = true;
    while (running && !g_stop) {
        for (int i = 0; i < S.len && !g_stop; ++i) {
            int fi = S.seq ? S.seq[i] : 0;
            for (int r = 0; r < F->logo_h_max; ++r) {
                term_move(r + 1, 1);
                const char *ln = (r < F->frames[fi].line_count) ? F->frames[fi].lines[r] : "";
                print_padded_logo_line(ln, F->logo_w_max);
                term_clear_to_eol();
            }
            fflush(stdout);
            msleep_int(frame_ms);

            if (duration > 0.0) {
                struct timespec t1;
                clock_gettime(CLOCK_MONOTONIC, &t1);
                elapsed = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec)/1e9;
                if (elapsed >= duration) { running = false; break; }
            }
        }
        if (mode == CFG_ANIM_ONCE) break;
        if (S.len == 0) break;
    }

    if (CFG_HIDE_CURSOR_DURING_ANIM) term_hide_cursor(false);
    if (CFG_USE_ALT_SCREEN) { term_alt_screen(false); fputc('\n', stdout); }
    free_sequence(&S);
}

// ---------- CLI parsing / main ----------
typedef struct {
    int layout;      // CFG_LAYOUT_SIDE / CFG_LAYOUT_TOP
    int anim_enabled;
    int anim_area;   // CFG_ANIM_LEFT / CFG_ANIM_TOP
    int anim_mode;   // OFF/LOOP/PINGPONG/ONCE
    int fps;
    double duration; // seconds
} RuntimeCfg;

static void usage(const char *argv0) {
    fprintf(stdout,
        "minifetch — tiny C system fetch with layout + ASCII animation\n"
        "Usage: %s [--layout=side|top] [--animate=off|left|top] [--mode=loop|pingpong|once]\n"
        "             [--fps=N] [--duration=SECONDS] [-h|--help]\n", argv0);
}

static void clamp_runtime(RuntimeCfg *R, bool is_tty) {
    if (R->fps < 1) R->fps = 1;
    if (R->fps > 60) R->fps = 60;
    if (R->duration < 0.0) R->duration = 0.0;
    if (R->duration > 24.0 * 3600.0) R->duration = 24.0 * 3600.0;
    if (CFG_ANIM_PLAY_ON_TTY_ONLY && !is_tty) {
        R->anim_enabled = 0;
        R->anim_mode = CFG_ANIM_OFF;
    }
    // Enforce valid area per layout
    if (R->layout == CFG_LAYOUT_SIDE) {
        if (R->anim_area != CFG_ANIM_LEFT) {
            fprintf(stderr, "Warning: animate area adjusted to LEFT for side layout.\n");
            R->anim_area = CFG_ANIM_LEFT;
        }
    } else {
        if (R->anim_area != CFG_ANIM_TOP) {
            fprintf(stderr, "Warning: animate area adjusted to TOP for top layout.\n");
            R->anim_area = CFG_ANIM_TOP;
        }
    }
}

int main(int argc, char **argv) {
    // Defaults from config.h
    RuntimeCfg R;
    R.layout = CFG_LAYOUT;
    R.anim_enabled = CFG_ANIM_ENABLED;
    R.anim_area = CFG_ANIM_AREA;
    R.anim_mode = CFG_ANIM_MODE;
    R.fps = CFG_ANIM_FPS;
    R.duration = CFG_ANIM_DURATION_SEC;

    for (int i = 1; i < argc; ++i) {
        if (!strncmp(argv[i], "--layout=", 9)) {
            const char *v = argv[i] + 9;
            if (!strcmp(v, "side")) R.layout = CFG_LAYOUT_SIDE;
            else if (!strcmp(v, "top")) R.layout = CFG_LAYOUT_TOP;
            else { usage(argv[0]); return 1; }
        } else if (!strncmp(argv[i], "--animate=", 10)) {
            const char *v = argv[i] + 10;
            if (!strcmp(v, "off")) { R.anim_enabled = 0; R.anim_mode = CFG_ANIM_OFF; }
            else if (!strcmp(v, "left")) { R.anim_enabled = 1; R.anim_area = CFG_ANIM_LEFT; }
            else if (!strcmp(v, "top"))  { R.anim_enabled = 1; R.anim_area = CFG_ANIM_TOP; }
            else { usage(argv[0]); return 1; }
        } else if (!strncmp(argv[i], "--mode=", 7)) {
            const char *v = argv[i] + 7;
            if (!strcmp(v, "loop")) R.anim_mode = CFG_ANIM_LOOP;
            else if (!strcmp(v, "pingpong")) R.anim_mode = CFG_ANIM_PINGPONG;
            else if (!strcmp(v, "once")) R.anim_mode = CFG_ANIM_ONCE;
            else { usage(argv[0]); return 1; }
        } else if (!strncmp(argv[i], "--fps=", 6)) {
            int n = atoi(argv[i] + 6);
            if (n > 0) R.fps = n;
        } else if (!strncmp(argv[i], "--duration=", 11)) {
            R.duration = atof(argv[i] + 11);
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(argv[0]); return 0;
        } else {
            usage(argv[0]); return 1;
        }
    }

    bool is_tty = term_is_tty();
    clamp_runtime(&R, is_tty);

    install_signal_handlers();

    Frames F = frames_parse_from_embed();
    InfoLines I = build_info_lines();

    // If not TTY, or animations disabled, print static (first frame only)
    if (!R.anim_enabled || R.anim_mode == CFG_ANIM_OFF || !is_tty) {
        if (R.layout == CFG_LAYOUT_SIDE) {
            render_static_side(&F, &I, CFG_GAP_SPACES);
        } else {
            render_static_top(&F, &I);
        }
        frames_free(&F);
        info_free(&I);
        return 0;
    }

    // Animated
    if (R.layout == CFG_LAYOUT_SIDE && R.anim_area == CFG_ANIM_LEFT) {
        animated_render_side(&F, &I, R.fps, R.duration, R.anim_mode);
    } else if (R.layout == CFG_LAYOUT_TOP && R.anim_area == CFG_ANIM_TOP) {
        animated_render_top(&F, &I, R.fps, R.duration, R.anim_mode);
    } else {
        // Should not happen after clamp; fallback static
        if (R.layout == CFG_LAYOUT_SIDE) render_static_side(&F, &I, CFG_GAP_SPACES);
        else render_static_top(&F, &I);
    }

    frames_free(&F);
    info_free(&I);
    return 0;
}
