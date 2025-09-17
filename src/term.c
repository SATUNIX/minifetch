#include <stdio.h>
#include <unistd.h>

#include "term.h"

#define MF_ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

int mf_is_tty(void)
{
    return isatty(STDOUT_FILENO) == 1;
}

void mf_format_bytes(double bytes, char *out, size_t outsz)
{
    static const char *units[] = { "B", "KiB", "MiB", "GiB", "TiB", "PiB" };
    int unit_index;
    double value;

    if (out == NULL || outsz == 0) {
        return;
    }

    if (bytes < 0.0) {
        bytes = 0.0;
    }

    value = bytes;
    unit_index = 0;

    while (value >= 1024.0 && unit_index < (int)(MF_ARRAY_LEN(units) - 1)) {
        value /= 1024.0;
        unit_index++;
    }

    if (value >= 100.0) {
        snprintf(out, outsz, "%.0f %s", value, units[unit_index]);
    } else if (value >= 10.0) {
        snprintf(out, outsz, "%.1f %s", value, units[unit_index]);
    } else {
        snprintf(out, outsz, "%.2f %s", value, units[unit_index]);
    }
}

size_t mf_utf8_display_width(const char *s)
{
    const unsigned char *p;
    size_t width;

    if (s == NULL) {
        return 0U;
    }

    width = 0U;
    p = (const unsigned char *)s;

    while (*p != '\0') {
        width++;
        if ((*p & 0x80U) == 0U) {
            p++;
        } else if ((*p & 0xE0U) == 0xC0U && p[1] != '\0') {
            p += 2;
        } else if ((*p & 0xF0U) == 0xE0U && p[1] != '\0' && p[2] != '\0') {
            p += 3;
        } else if ((*p & 0xF8U) == 0xF0U && p[1] != '\0' && p[2] != '\0' && p[3] != '\0') {
            p += 4;
        } else {
            p++;
        }
    }

    return width;
}
