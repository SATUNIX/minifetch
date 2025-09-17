#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "cli.h"
#include "core.h"
#include "linux_extras.h"
#include "compat.h"
#include "logo.h"
#include "term.h"
#include "hidden.h"

#define MF_ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

struct mf_field {
    const char *label;
    int (*collector)(char *out, size_t outsz);
    int enabled_default;
};

static const struct mf_field g_fields[] = {
    { "OS:",     mf_collect_os,     CFG_SHOW_OS },
    { "Kernel:", mf_collect_kernel, CFG_SHOW_KERNEL },
    { "Host:",   mf_collect_host,   CFG_SHOW_HOST },
    { "CPU:",    mf_collect_cpu,    CFG_SHOW_CPU },
    { "Shell:",  mf_collect_shell,  CFG_SHOW_SHELL },
    { "Disk:",   mf_collect_disk,   CFG_SHOW_DISK },
    { "Memory:", mf_collect_mem,    CFG_SHOW_MEM },
    { "Uptime:", mf_collect_uptime, CFG_SHOW_UPTIME }
};

struct mf_render_line {
    const char *label;
    char value[256];
};

int main(int argc, char **argv)
{
    struct mf_options opts;
    struct mf_render_line lines[MF_ARRAY_LEN(g_fields)];
    char formatted[MF_ARRAY_LEN(g_fields)][MF_FORMATTED_LINE_MAX];
    size_t visible_widths[MF_ARRAY_LEN(g_fields)];
    size_t line_count;
    int parse_result;
    int want_colour;
    int stdout_is_tty;
    const char *label_colour;
    const char *value_colour;
    const char *reset_colour;
    size_t i;

    parse_result = mf_cli_parse(argc, argv, &opts);
    if (parse_result != 0) {
        mf_cli_print_usage(argv[0]);
        return 1;
    }

    if (opts.help != 0) {
        mf_cli_print_usage(argv[0]);
        return 0;
    }

    stdout_is_tty = mf_is_tty();
    want_colour = stdout_is_tty;
    if (opts.no_colour) {
        want_colour = 0;
    }
    if (opts.hidden) {
        want_colour = 0;
    }

    label_colour = want_colour ? CFG_LABEL_COLOR : "";
    value_colour = want_colour ? CFG_VALUE_COLOR : "";
    reset_colour = want_colour ? CFG_RESET_COLOR : "";

    line_count = 0;
    for (i = 0; i < MF_ARRAY_LEN(g_fields); ++i) {
        int enabled;
        int rc;

        enabled = g_fields[i].enabled_default;
        if (!enabled && !opts.show_all) {
            continue;
        }

        rc = g_fields[i].collector(lines[line_count].value, sizeof(lines[line_count].value));
        if (rc != 0) {
            continue;
        }

        lines[line_count].label = g_fields[i].label;

        if (opts.quiet) {
            mf_strlcpy(formatted[line_count], lines[line_count].value, MF_FORMATTED_LINE_MAX);
            visible_widths[line_count] = mf_utf8_display_width(lines[line_count].value);
        } else {
            snprintf(formatted[line_count], MF_FORMATTED_LINE_MAX,
                     "%s%-*s%s %s%s%s",
                     label_colour,
                     CFG_LABEL_WIDTH,
                     lines[line_count].label,
                     reset_colour,
                     value_colour,
                     lines[line_count].value,
                     reset_colour);
            visible_widths[line_count] = CFG_LABEL_WIDTH + 1U + mf_utf8_display_width(lines[line_count].value);
        }

        line_count++;
    }

    if (line_count == 0 && g_logo_line_count == 0U) {
        return 0;
    }

    if (opts.hidden) {
        if (!stdout_is_tty) {
            opts.hidden = 0;
        }
    }

    if (opts.hidden) {
        mf_run_hidden_mode(formatted, visible_widths, line_count, opts.quiet);
        return 0;
    }

    {
        size_t rows;
        size_t column_gap;

        rows = line_count;
        if (g_logo_line_count > rows) {
            rows = g_logo_line_count;
        }

        column_gap = 0U;
        if (line_count > 0 && g_logo_line_count > 0) {
            column_gap = 2U;
        }

        for (i = 0; i < rows; ++i) {
            size_t pad_spaces;
            size_t n;

            if (i < g_logo_line_count) {
                const char *logo_line = g_logo_lines[i];
                size_t logo_len = mf_utf8_display_width(logo_line);
                fputs(logo_line, stdout);
                if (g_logo_width > logo_len) {
                    pad_spaces = g_logo_width - logo_len;
                } else {
                    pad_spaces = 0U;
                }
            } else {
                pad_spaces = g_logo_width;
            }

            n = pad_spaces;
            while (n > 0U) {
                fputc(' ', stdout);
                n--;
            }

            if (i < line_count) {
                n = column_gap;
                while (n > 0U) {
                    fputc(' ', stdout);
                    n--;
                }
                fputs(formatted[i], stdout);
            }

            fputc('\n', stdout);
        }
    }

    return 0;
}
