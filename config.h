// config.h — compile-time defaults for minifetch (no ASCII art here)
//
// Tweak these values to change the information that is collected, the
// default colour palette, or the initial layout / animation behaviour. Most
// animation-related settings can also be overridden at runtime via CLI flags.

/* ---------------- Feature toggles ---------------- */
// Set to 1 to include the field, 0 to omit it from the info column.
#define CFG_SHOW_DISTRO 1
#define CFG_SHOW_HOST   1
#define CFG_SHOW_KERNEL 1
#define CFG_SHOW_UPTIME 1
#define CFG_SHOW_PKGS   1
#define CFG_SHOW_SHELL  1
#define CFG_SHOW_RES    0
#define CFG_SHOW_WM     1
#define CFG_SHOW_DE     1
#define CFG_SHOW_CPU    1
#define CFG_SHOW_GPU    1
#define CFG_SHOW_MEM    1
#define CFG_SHOW_DISK   1

/* ---------------- Colours & spacing ---------------- */
// Provide ANSI escape sequences – the renderer automatically appends \x1b[0m
// to reset colours after each label/value pair.
#define CFG_LOGO_COLOR  "\x1b[38;5;39m"   /* cyan-ish logo tint */
#define CFG_LABEL_COLOR "\x1b[38;5;245m"  /* dim grey labels */
#define CFG_VALUE_COLOR "\x1b[38;5;252m"  /* light values */

#define CFG_GAP_SPACES  4  /* >= 1, horizontal gap between logo and info in SIDE layout */

/* ---------------- Layout defaults ---------------- */
// Do not change these enum values – other code compares against them.
#define CFG_LAYOUT_SIDE 0
#define CFG_LAYOUT_TOP  1

// Default layout when the program starts (CLI --layout overrides this).
#define CFG_LAYOUT CFG_LAYOUT_SIDE

/* ---------------- Animation defaults ---------------- */
// Animation areas map to the logo location: left column for SIDE layout,
// whole top block for TOP layout. Keep the enum values unique.
#define CFG_ANIM_LEFT 0
#define CFG_ANIM_TOP  1

#define CFG_ANIM_OFF      0
#define CFG_ANIM_LOOP     1
#define CFG_ANIM_PINGPONG 2
#define CFG_ANIM_ONCE     3

// Enable animation by default? (CLI --animate can override at runtime.)
#define CFG_ANIM_ENABLED 1

// Primary animation area to start with; should match CFG_LAYOUT above. When
// mismatched, runtime code coerces the area and prints a warning once.
#define CFG_ANIM_AREA CFG_ANIM_LEFT

// Default animation mode. LOOP is a continuous forward sequence; PINGPONG
#define CFG_ANIM_MODE CFG_ANIM_LOOP

// Nominal FPS before clamping to [1, 60].
#define CFG_ANIM_FPS 10

// Max duration in seconds (0 = infinite for LOOP/PINGPONG). Runtime clips to 24h.
#define CFG_ANIM_DURATION_SEC 0.0

// If stdout is not a TTY and this is 1, animation is suppressed and the first
// frame is printed statically. Useful when piping to files or pagers.
#define CFG_ANIM_PLAY_ON_TTY_ONLY 1

// Opt-in terminal niceties while animating.
#define CFG_USE_ALT_SCREEN 1        /* switch to alt screen buffer */
#define CFG_HIDE_CURSOR_DURING_ANIM 1 /* hide cursor for cleaner output */
