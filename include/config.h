/* config.h — compile-time defaults for the streamlined minifetch */
#ifndef MINIFETCH_CONFIG_H
#define MINIFETCH_CONFIG_H

/* ---------------- Feature toggles ---------------- */
#define CFG_SHOW_OS      1
#define CFG_SHOW_KERNEL  1
#define CFG_SHOW_HOST    1
#define CFG_SHOW_CPU     1
#define CFG_SHOW_SHELL   1
#define CFG_SHOW_DISK    1
#ifdef MINIFETCH_LINUX_EXT
#define CFG_SHOW_MEM     1
#define CFG_SHOW_UPTIME  1
#else
#define CFG_SHOW_MEM     0
#define CFG_SHOW_UPTIME  0
#endif

/* ---------------- Colour palette ---------------- */
#define CFG_LABEL_COLOR  "\x1b[38;5;245m"
#define CFG_VALUE_COLOR  "\x1b[38;5;252m"
#define CFG_RESET_COLOR  "\x1b[0m"

/* Width (in characters) used when padding labels before the value column. */
#define CFG_LABEL_WIDTH  10

#endif /* MINIFETCH_CONFIG_H */
