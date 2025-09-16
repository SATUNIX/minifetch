// config.h - tweak colors, fields, spacing, and ASCII art

#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

// Which fields to show (1 = show, 0 = hide)
#define CFG_SHOW_DISTRO   1
#define CFG_SHOW_HOST     1
#define CFG_SHOW_KERNEL   1
#define CFG_SHOW_UPTIME   1
#define CFG_SHOW_PKGS     1
#define CFG_SHOW_SHELL    1
#define CFG_SHOW_RES      1
#define CFG_SHOW_WM       1
#define CFG_SHOW_DE       0   // hide GTK/theme stuff by default; DE off, WM on
#define CFG_SHOW_CPU      1
#define CFG_SHOW_GPU      1
#define CFG_SHOW_MEM      1
#define CFG_SHOW_DISK     1

// Spacing between ASCII art and info
#define CFG_GAP_SPACES    4

// ANSI colors (logo / labels / values)
#define CFG_LOGO_COLOR  "\x1b[34m"   // blue
#define CFG_LABEL_COLOR "\x1b[37m"   // white (labels)
#define CFG_VALUE_COLOR "\x1b[0m"    // default for values

// Example ASCII logo (Arch Linux)
static const char *CFG_LOGO[] = {
"",
"   █████████   ",
"  ███░░░░░███  ",
" ░███    ░███  ",
" ░███████████  ",
" ░███░░░░░███  ",
" ░███    ░███  ",
" █████   █████ ",
"░░░░░   ░░░░░  ",
"",             
NULL
};
//Thanks to asciiart.eu 
//jgs 
#endif // CONFIG_H_INCLUDED

