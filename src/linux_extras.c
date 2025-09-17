#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "linux_extras.h"
#include "compat.h"
#include "term.h"

#if defined(__linux__) && defined(MINIFETCH_LINUX_EXT)
static long mf_parse_kib_value(const char *line, const char *label)
{
    size_t len;
    const char *p;
    char *endptr;
    long value;

    len = strlen(label);
    if (strncmp(line, label, len) != 0) {
        return -1;
    }

    p = line + len;
    while (*p == ' ' || *p == '\t') {
        p++;
    }

    value = strtol(p, &endptr, 10);
    if (endptr == NULL) {
        return -1;
    }
    return value;
}
#endif

int mf_collect_mem(char *out, size_t outsz)
{
#if defined(__linux__) && defined(MINIFETCH_LINUX_EXT)
    FILE *fp;
    char line[256];
    long total_kib;
    long avail_kib;
    long free_kib;
    long buffers_kib;
    long cached_kib;
    double used_bytes;
    double total_bytes;
    char used_buf[64];
    char total_buf[64];

    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        return -1;
    }

    total_kib = -1;
    avail_kib = -1;
    free_kib = -1;
    buffers_kib = -1;
    cached_kib = -1;

    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        long value;
        value = mf_parse_kib_value(line, "MemTotal:");
        if (value >= 0) {
            total_kib = value;
            continue;
        }
        value = mf_parse_kib_value(line, "MemAvailable:");
        if (value >= 0) {
            avail_kib = value;
            continue;
        }
        value = mf_parse_kib_value(line, "MemFree:");
        if (value >= 0) {
            free_kib = value;
            continue;
        }
        value = mf_parse_kib_value(line, "Buffers:");
        if (value >= 0) {
            buffers_kib = value;
            continue;
        }
        value = mf_parse_kib_value(line, "Cached:");
        if (value >= 0) {
            cached_kib = value;
            continue;
        }
    }

    fclose(fp);

    if (total_kib <= 0) {
        return -1;
    }

    if (avail_kib < 0) {
        if (free_kib >= 0 && buffers_kib >= 0 && cached_kib >= 0) {
            avail_kib = free_kib + buffers_kib + cached_kib;
        }
    }

    if (avail_kib < 0) {
        return -1;
    }

    used_bytes = ((double)total_kib - (double)avail_kib) * 1024.0;
    if (used_bytes < 0.0) {
        used_bytes = 0.0;
    }
    total_bytes = (double)total_kib * 1024.0;

    mf_format_bytes(used_bytes, used_buf, sizeof(used_buf));
    mf_format_bytes(total_bytes, total_buf, sizeof(total_buf));

    snprintf(out, outsz, "%s / %s", used_buf, total_buf);
    return 0;
#else
    (void)out;
    (void)outsz;
    return -1;
#endif
}

int mf_collect_uptime(char *out, size_t outsz)
{
#if defined(__linux__) && defined(MINIFETCH_LINUX_EXT)
    FILE *fp;
    char line[128];
    double seconds;
    char *endptr;
    long total_minutes;
    long days;
    long hours;
    long minutes;

    fp = fopen("/proc/uptime", "r");
    if (fp == NULL) {
        return -1;
    }

    if (fgets(line, (int)sizeof(line), fp) == NULL) {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    seconds = strtod(line, &endptr);
    if (endptr == line || seconds < 0.0) {
        return -1;
    }

    total_minutes = (long)(seconds / 60.0);
    if (total_minutes < 0) {
        total_minutes = 0;
    }

    days = total_minutes / (60 * 24);
    total_minutes -= days * 60 * 24;
    hours = total_minutes / 60;
    minutes = total_minutes - hours * 60;

    if (days > 0) {
        snprintf(out, outsz, "%ldd %ldh %ldm", days, hours, minutes);
    } else if (hours > 0) {
        snprintf(out, outsz, "%ldh %ldm", hours, minutes);
    } else {
        snprintf(out, outsz, "%ldm", minutes);
    }
    return 0;
#else
    (void)out;
    (void)outsz;
    return -1;
#endif
}
