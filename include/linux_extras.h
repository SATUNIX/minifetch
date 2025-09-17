#ifndef MINIFETCH_LINUX_EXTRAS_H
#define MINIFETCH_LINUX_EXTRAS_H

#include <stddef.h>

int mf_collect_mem(char *out, size_t outsz);
int mf_collect_uptime(char *out, size_t outsz);

#endif /* MINIFETCH_LINUX_EXTRAS_H */
