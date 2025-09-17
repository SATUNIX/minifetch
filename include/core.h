#ifndef MINIFETCH_CORE_H
#define MINIFETCH_CORE_H

#include <stddef.h>

int mf_collect_os(char *out, size_t outsz);
int mf_collect_kernel(char *out, size_t outsz);
int mf_collect_host(char *out, size_t outsz);
int mf_collect_cpu(char *out, size_t outsz);
int mf_collect_shell(char *out, size_t outsz);
int mf_collect_disk(char *out, size_t outsz);

#endif /* MINIFETCH_CORE_H */
