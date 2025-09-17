#include <stddef.h>
#include <string.h>

#include "compat.h"

size_t mf_strlcpy(char *dst, const char *src, size_t dstsz)
{
    size_t i;
    size_t len;

    if (dst == NULL || dstsz == 0) {
        return 0;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return 0;
    }

    len = strlen(src);
    if (len >= dstsz) {
        len = dstsz - 1;
    }

    for (i = 0; i < len; ++i) {
        dst[i] = src[i];
    }
    dst[len] = '\0';

    return len;
}

void mf_rstrip(char *s)
{
    size_t len;

    if (s == NULL) {
        return;
    }

    len = strlen(s);
    while (len > 0) {
        char c;
        c = s[len - 1];
        if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
            s[len - 1] = '\0';
            len--;
        } else {
            break;
        }
    }
}

void mf_unquote(char *s)
{
    size_t len;

    if (s == NULL) {
        return;
    }

    mf_rstrip(s);
    len = strlen(s);
    if (len >= 2) {
        char first;
        char last;
        first = s[0];
        last = s[len - 1];
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            size_t i;
            for (i = 0; i < len - 2; ++i) {
                s[i] = s[i + 1];
            }
            s[len - 2] = '\0';
        }
    }
}
