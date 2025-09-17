#ifndef MINIFETCH_HIDDEN_H
#define MINIFETCH_HIDDEN_H

#include <stddef.h>

#define MF_FORMATTED_LINE_MAX 512

int mf_run_hidden_mode(char formatted[][MF_FORMATTED_LINE_MAX], const size_t widths[], size_t count, int quiet_mode);

#endif /* MINIFETCH_HIDDEN_H */
