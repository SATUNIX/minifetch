// minifetch.c - ultra-minimal screenfetch-like tool (C, GCC)
// Arch & Debian-based support, Wayland/Hyprland aware. Prints ASCII art + info side-by-side.
// Build: gcc -O2 -pipe -s -o minifetch minifetch.c
// License: MIT

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <dirent.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "config.h"  // User-configurable options + ASCII art

// ---------- Utilities ----------
static char *trim(char *s) {
    if (!s) return s;
    size_t len = strlen(s);
    while (len && (s[len-1]=='\n' || s[len-1]=='\r' || isspace((unsigned char)s[len-1]))) s[--len]=0;
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static bool cmd_exists(const char *name) {
    // very small 'which' check using PATH
    const char *path = getenv("PATH");
    if (!path) return false;
    char *paths = strdup(path);
    if (!paths) return false;
    char *tok = strtok(paths, ":");
    char candidate[1024];
    bool found = false;
    while (tok) {
        snprintf(candidate, sizeof(candidate), "%s/%s", tok, name);
        if (access(candidate, X_OK) == 0) { found = true; break; }
        tok = strtok(NULL, ":");
    }
    free(paths);
    return found;
}

static char *slurp_first_line(const char *path, char *buf, size_t bufsz) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    if (!fgets(buf, (int)bufsz, f)) { fclose(f); return NULL; }
    fclose(f);
    return trim(buf);
}

static char *read_kv_from_file(const char *path, const char *key, char *out, size_t outsz) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char line[4096];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, klen)==0 && line[klen]=='=') {
            char *val = line + klen + 1;
            val = trim(val);
            // strip quotes if present
            if (val[0]=='\"') {
                size_t n = strlen(val);
                if (n>1 && val[n-1]=='\"') { val[n-1]=0; val++; }
            }
            strncpy(out, val, outsz-1);
            out[outsz-1]=0;
            fclose(f);
            return out;
        }
    }
    fclose(f);
    return NULL;
}

static char *exec_read_first_line(const char *cmd, char *buf, size_t bufsz) {
    FILE *p = popen(cmd, "r");
    if (!p) return NULL;
    if (!fgets(buf, (int)bufsz, p)) { pclose(p); return NULL; }
    pclose(p);
    return trim(buf);
}

static void human_bytes(unsigned long long bytes, char *out, size_t outsz) {
    const char *units[] = {"B","KiB","MiB","GiB","TiB","PiB"};
    int i=0;
    double b = (double)bytes;
    while (b>=1024.0 && i<5) { b/=1024.0; i++; }
    snprintf(out, outsz, (b<10.0) ? "%.2f %s" : "%.0f %s", b, units[i]);
}

// ---------- Collectors ----------
static void get_distro(char *out, size_t n) {
    if (read_kv_from_file("/etc/os-release", "PRETTY_NAME", out, n)) return;
    if (read_kv_from_file("/usr/lib/os-release", "PRETTY_NAME", out, n)) return;
    strncpy(out, "Linux", n-1); out[n-1]=0;
}

static void get_host(char *out, size_t n) {
    if (slurp_first_line("/etc/hostname", out, n)) return;
    struct utsname u;
    if (uname(&u)==0) { snprintf(out, n, "%s", u.nodename); return; }
    strncpy(out, "unknown", n-1); out[n-1]=0;
}

static void get_kernel(char *out, size_t n) {
    struct utsname u;
    if (uname(&u)==0) { snprintf(out, n, "%s", u.release); return; }
    strncpy(out, "unknown", n-1); out[n-1]=0;
}

static void get_uptime(char *out, size_t n) {
    FILE *f = fopen("/proc/uptime", "r");
    if (!f) { strncpy(out, "unknown", n-1); out[n-1]=0; return; }
    double up=0; if (fscanf(f, "%lf", &up)!=1) { fclose(f); strncpy(out,"unknown",n-1); out[n-1]=0; return; }
    fclose(f);
    unsigned long seconds = (unsigned long)up;
    unsigned days = seconds/86400; seconds%=86400;
    unsigned hours = seconds/3600; seconds%=3600;
    unsigned mins = seconds/60;
    if (days) snprintf(out, n, "%ud %uh %um", days, hours, mins);
    else if (hours) snprintf(out, n, "%uh %um", hours, mins);
    else snprintf(out, n, "%um", mins);
}

static void get_packages(char *out, size_t n) {
    // try pacman
    if (cmd_exists("pacman")) {
        if (exec_read_first_line("sh -c \"pacman -Qq 2>/dev/null | wc -l\"", out, n)) return;
    }
    // try dpkg
    if (cmd_exists("dpkg")) {
        if (exec_read_first_line("sh -c \"dpkg -l 2>/dev/null | grep -E '^ii|^hi' | wc -l\"", out, n)) return;
    }
    // try dpkg-query (fallback)
    if (cmd_exists("dpkg-query")) {
        if (exec_read_first_line("sh -c \"dpkg-query -f '.\\n' -W 2>/dev/null | wc -l\"", out, n)) return;
    }
    strncpy(out, "n/a", n-1); out[n-1]=0;
}

static void get_shell(char *out, size_t n) {
    // Hardened: do NOT spawn any commands; just report the basename of $SHELL
    const char *sh = getenv("SHELL");
    if (!sh || !*sh) { snprintf(out, n, "n/a"); return; }
    const char *base = strrchr(sh, '/');
    base = base ? base + 1 : sh;

    // Ensure base contains only printable, safe characters; fallback to "unknown" if weird
    size_t len = strnlen(base, 127);
    bool safe = len > 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)base[i];
        if (!(isalnum(c) || c=='_' || c=='-' || c=='.')) { safe = false; break; }
    }
    if (!safe) snprintf(out, n, "unknown");
    else snprintf(out, n, "%s", base);
}

static void get_resolution(char *out, size_t n) {
    // Wayland/Hyprland first
    if (getenv("HYPRLAND_INSTANCE_SIGNATURE") && cmd_exists("hyprctl")) {
        if (exec_read_first_line("sh -c \"hyprctl monitors -j 2>/dev/null | sed -n 's/.*\\\"width\\\":\\([0-9]*\\).*\\\"height\\\":\\([0-9]*\\).*/\\1x\\2/p' | paste -sd, -\"", out, n)) return;
    }
    if (getenv("WAYLAND_DISPLAY")) {
        if (cmd_exists("wlr-randr") &&
            exec_read_first_line("sh -c \"wlr-randr 2>/dev/null | awk '/current/ {print $1}' | paste -sd, -\"", out, n)) return;
        if (cmd_exists("wayland-info") &&
            exec_read_first_line("sh -c \"wayland-info 2>/dev/null | sed -n 's/.*current_extent.\\([0-9]*\\) x \\([0-9]*\\).*/\\1x\\2/p' | paste -sd, -\"", out, n)) return;
    }
    // X11
    if (cmd_exists("xrandr")) {
        if (exec_read_first_line("sh -c \"xrandr 2>/dev/null | awk '/\\*/ {print $1}' | paste -sd, -\"", out, n)) return;
    }
    strncpy(out, "n/a", n-1); out[n-1]=0;
}

static void get_wm_de(char *wm, size_t wn, char *de, size_t dn) {
    // WM
    if (getenv("HYPRLAND_INSTANCE_SIGNATURE")) snprintf(wm, wn, "Hyprland");
    else if (getenv("SWAYSOCK")) snprintf(wm, wn, "Sway");
    else if (getenv("XDG_CURRENT_DESKTOP")) snprintf(wm, wn, "X11/Wayland");
    else snprintf(wm, wn, "n/a");

    // DE
    const char *cde = getenv("XDG_CURRENT_DESKTOP");
    const char *dse = getenv("DESKTOP_SESSION");
    if (cde && *cde) snprintf(de, dn, "%s", cde);
    else if (dse && *dse) snprintf(de, dn, "%s", dse);
    else snprintf(de, dn, "n/a");
}

static void get_cpu(char *out, size_t n) {
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) { strncpy(out, "n/a", n-1); out[n-1]=0; return; }
    char line[4096], model[256]="";
    int cores=0;
    while (fgets(line, sizeof(line), f)) {
        if (!model[0]) {
            if (strncmp(line, "model name", 10)==0) {
                char *colon = strchr(line, ':'); if (colon) snprintf(model, sizeof(model), "%s", trim(colon+1));
            }
        }
        if (strncmp(line, "processor", 9)==0) cores++; // logical cores
    }
    fclose(f);
    if (model[0]) snprintf(out, n, "%s (%d)", model, cores?cores:1);
    else snprintf(out, n, "n/a");
}

static void get_gpu(char *out, size_t n) {
    if (cmd_exists("lspci")) {
        if (exec_read_first_line("sh -c \"lspci 2>/dev/null | grep -iE 'vga|3d|display' | sed 's/.*: //; s/ (rev.*)//' | paste -sd, -\"", out, n)) return;
    }
    strncpy(out, "n/a", n-1); out[n-1]=0;
}

static void get_mem(char *out, size_t n) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) { strncpy(out, "n/a", n-1); out[n-1]=0; return; }
    unsigned long long total=0, avail=0;
    char k[64]; unsigned long long v;
    while (fscanf(f, "%63s %llu kB\n", k, &v)==2) {
        if (strcmp(k, "MemTotal:")==0) total = v*1024ULL;
        else if (strcmp(k, "MemAvailable:")==0) avail = v*1024ULL;
    }
    fclose(f);
    if (!total) { strncpy(out, "n/a", n-1); out[n-1]=0; return; }
    unsigned long long used = total - avail;
    char th[64], uh[64];
    human_bytes(used, uh, sizeof(uh));
    human_bytes(total, th, sizeof(th));
    snprintf(out, n, "%s / %s", uh, th);
}

static void get_disk_root(char *out, size_t n) {
    struct statvfs s;
    if (statvfs("/", &s)!=0) { strncpy(out, "n/a", n-1); out[n-1]=0; return; }
    unsigned long long total = (unsigned long long)s.f_frsize * s.f_blocks;
    unsigned long long freeb = (unsigned long long)s.f_frsize * s.f_bavail;
    unsigned long long used = total - freeb;
    char th[64], uh[64];
    human_bytes(used, uh, sizeof(uh));
    human_bytes(total, th, sizeof(th));
    int pct = total ? (int)((used * 100.0) / (double)total) : 0;
    snprintf(out, n, "%s / %s (%d%%)", uh, th, pct);
}

// ---------- Rendering ----------
static void print_line(const char *left, const char *right, int logo_w) {
    const char *lc = CFG_LOGO_COLOR, *nc = "\x1b[0m";
    int leftlen = (int)strlen(left);
    int pad = logo_w - leftlen;
    if (pad < 0) pad = 0;

    // left (logo) colored + padding + fixed gap + right (already colored inside string)
    printf("%s%s%s%*s%*s", lc, left, nc, pad, "", CFG_GAP_SPACES, "");
    if (right && *right) printf("%s\n", right);
    else puts("");
}

int main(void) {
    // Collect data into strings
    char distro[256], host[256], kernel[256], uptime[128], pkgs[64], shell[256];
    char res[256], wm[128], de[128], cpu[512], gpu[512], mem[160], disk[160];

    if (CFG_SHOW_DISTRO)  get_distro(distro, sizeof(distro));
    if (CFG_SHOW_HOST)    get_host(host, sizeof(host));
    if (CFG_SHOW_KERNEL)  get_kernel(kernel, sizeof(kernel));
    if (CFG_SHOW_UPTIME)  get_uptime(uptime, sizeof(uptime));
    if (CFG_SHOW_PKGS)    get_packages(pkgs, sizeof(pkgs));
    if (CFG_SHOW_SHELL)   get_shell(shell, sizeof(shell));
    if (CFG_SHOW_RES)     get_resolution(res, sizeof(res));
    if (CFG_SHOW_WM || CFG_SHOW_DE) get_wm_de(wm, sizeof(wm), de, sizeof(de));
    if (CFG_SHOW_CPU)     get_cpu(cpu, sizeof(cpu));
    if (CFG_SHOW_GPU)     get_gpu(gpu, sizeof(gpu));
    if (CFG_SHOW_MEM)     get_mem(mem, sizeof(mem));
    if (CFG_SHOW_DISK)    get_disk_root(disk, sizeof(disk));

    // Build formatted info lines with embedded colors
    const char *info[32];
    int nlines = 0;
    char linebuf[32][640];

    #define ADD(L,V) do { snprintf(linebuf[nlines], sizeof(linebuf[nlines]), "%s%s\x1b[0m %s%s\x1b[0m", CFG_LABEL_COLOR, (L), CFG_VALUE_COLOR, (V)); info[nlines]=linebuf[nlines]; nlines++; } while(0)

    if (CFG_SHOW_DISTRO) ADD("OS:", distro);
    if (CFG_SHOW_HOST)   ADD("Host:", host);
    if (CFG_SHOW_KERNEL) ADD("Kernel:", kernel);
    if (CFG_SHOW_UPTIME) ADD("Uptime:", uptime);
    if (CFG_SHOW_PKGS)   ADD("Packages:", pkgs);
    if (CFG_SHOW_SHELL)  ADD("Shell:", shell);
    if (CFG_SHOW_RES)    ADD("Resolution:", res);
    if (CFG_SHOW_WM)     ADD("WM:", wm);
    if (CFG_SHOW_DE)     ADD("DE:", de);
    if (CFG_SHOW_CPU)    ADD("CPU:", cpu);
    if (CFG_SHOW_GPU)    ADD("GPU:", gpu);
    if (CFG_SHOW_MEM)    ADD("Memory:", mem);
    if (CFG_SHOW_DISK)   ADD("Disk (/):", disk);

    // Determine logo column width
    int logo_h = 0, logo_w = 0;
    for (int i=0; CFG_LOGO[i]; i++) {
        int w = (int)strlen(CFG_LOGO[i]);
        if (w > logo_w) logo_w = w;
        logo_h++;
    }

    // Side-by-side output
    int max_rows = (logo_h > nlines) ? logo_h : nlines;
    for (int r=0; r<max_rows; r++) {
        const char *left = (r < logo_h) ? CFG_LOGO[r] : "";
        const char *right = (r < nlines) ? info[r] : "";
        print_line(left, right, logo_w);
    }

    return 0;
}

