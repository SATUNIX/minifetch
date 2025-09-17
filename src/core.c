#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "core.h"
#include "compat.h"
#include "term.h"

#if defined(__linux__) && defined(MINIFETCH_LINUX_EXT)
static int mf_linux_read_os_release(char *out, size_t outsz)
{
    const char *candidates[] = { "/etc/os-release", "/usr/lib/os-release" };
    size_t i;

    for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        FILE *fp;
        char line[256];
        char pretty[256];
        char name[256];

        pretty[0] = '\0';
        name[0] = '\0';

        fp = fopen(candidates[i], "r");
        if (fp == NULL) {
            continue;
        }

        while (fgets(line, (int)sizeof(line), fp) != NULL) {
            if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
                char *value;
                value = line + 12;
                mf_unquote(value);
                mf_strlcpy(pretty, value, sizeof(pretty));
                break;
            }
            if (name[0] == '\0' && strncmp(line, "NAME=", 5) == 0) {
                char *value;
                value = line + 5;
                mf_unquote(value);
                mf_strlcpy(name, value, sizeof(name));
            }
        }

        fclose(fp);

        if (pretty[0] != '\0') {
            mf_strlcpy(out, pretty, outsz);
            return 0;
        }
        if (name[0] != '\0') {
            mf_strlcpy(out, name, outsz);
            return 0;
        }
    }

    return -1;
}
#endif

int mf_collect_os(char *out, size_t outsz)
{
    struct utsname info;

#if defined(__linux__) && defined(MINIFETCH_LINUX_EXT)
    if (mf_linux_read_os_release(out, outsz) == 0) {
        return 0;
    }
#endif

    if (uname(&info) == 0) {
        mf_strlcpy(out, info.sysname, outsz);
        return 0;
    }

    (void)out;
    (void)outsz;
    return -1;
}

int mf_collect_kernel(char *out, size_t outsz)
{
    struct utsname info;

    if (uname(&info) != 0) {
        return -1;
    }

    mf_strlcpy(out, info.release, outsz);
    return 0;
}

int mf_collect_host(char *out, size_t outsz)
{
    FILE *fp;
    struct utsname info;

    fp = fopen("/etc/hostname", "r");
    if (fp != NULL) {
        if (fgets(out, (int)outsz, fp) != NULL) {
            mf_rstrip(out);
            fclose(fp);
            if (out[0] != '\0') {
                return 0;
            }
        } else {
            fclose(fp);
        }
    }

    if (uname(&info) == 0) {
        mf_strlcpy(out, info.nodename, outsz);
        return 0;
    }

    return -1;
}

int mf_collect_cpu(char *out, size_t outsz)
{
    long cpus;

    cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpus < 1) {
        cpus = 1;
    }

    snprintf(out, outsz, "%ld", cpus);
    return 0;
}

int mf_collect_shell(char *out, size_t outsz)
{
    const char *env_shell;
    const char *base;
    char tmp[128];
    size_t i;

    env_shell = getenv("SHELL");
    if (env_shell == NULL || env_shell[0] == '\0') {
        return -1;
    }

    base = strrchr(env_shell, '/');
    if (base != NULL && base[1] != '\0') {
        base++;
    } else {
        base = env_shell;
    }

    mf_strlcpy(tmp, base, sizeof(tmp));

    for (i = 0; tmp[i] != '\0'; ++i) {
        unsigned char c;
        c = (unsigned char)tmp[i];
        if (!(isalnum(c) || c == '_' || c == '.' || c == '-')) {
            tmp[i] = '_';
        }
    }

    if (tmp[0] == '\0') {
        return -1;
    }

    mf_strlcpy(out, tmp, outsz);
    return 0;
}

int mf_collect_disk(char *out, size_t outsz)
{
    struct statvfs vfs;
    double total_bytes;
    double available_bytes;
    double used_bytes;
    double percent;
    char used_buf[64];
    char total_buf[64];

    if (statvfs("/", &vfs) != 0) {
        return -1;
    }

    total_bytes = (double)vfs.f_blocks * (double)vfs.f_frsize;
    available_bytes = (double)vfs.f_bavail * (double)vfs.f_frsize;
    used_bytes = total_bytes - available_bytes;
    if (total_bytes <= 0.0) {
        return -1;
    }
    if (used_bytes < 0.0) {
        used_bytes = 0.0;
    }

    percent = (used_bytes / total_bytes) * 100.0;

    mf_format_bytes(used_bytes, used_buf, sizeof(used_buf));
    mf_format_bytes(total_bytes, total_buf, sizeof(total_buf));

    snprintf(out, outsz, "%s / %s (%.0f%%)", used_buf, total_buf, percent);
    return 0;
}
