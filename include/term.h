#ifndef MINIFETCH_TERM_H
#define MINIFETCH_TERM_H

#include <stddef.h>

int mf_is_tty(void);
void mf_format_bytes(double bytes, char *out, size_t outsz);
size_t mf_utf8_display_width(const char *s);

#endif /* MINIFETCH_TERM_H */
