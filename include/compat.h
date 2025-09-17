#ifndef MINIFETCH_COMPAT_H
#define MINIFETCH_COMPAT_H

#include <stddef.h>

size_t mf_strlcpy(char *dst, const char *src, size_t dstsz);
void mf_rstrip(char *s);
void mf_unquote(char *s);

#endif /* MINIFETCH_COMPAT_H */
