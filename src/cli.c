#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cli.h"

int mf_cli_parse(int argc, char **argv, struct mf_options *opts)
{
    int ch;

    if (opts == NULL) {
        return -1;
    }

    opts->show_all = 0;
    opts->no_colour = 0;
    opts->quiet = 0;
    opts->help = 0;

    opterr = 0;

    while ((ch = getopt(argc, argv, "achq")) != -1) {
        switch (ch) {
        case 'a':
            opts->show_all = 1;
            break;
        case 'c':
            opts->no_colour = 1;
            break;
        case 'h':
            opts->help = 1;
            break;
        case 'q':
            opts->quiet = 1;
            break;
        case '?':
        default:
            return -1;
        }
    }

    if (optind != argc) {
        return -1;
    }

    return 0;
}

void mf_cli_print_usage(const char *prog)
{
    const char *name;

    if (prog == NULL) {
        name = "minifetch";
    } else {
        name = prog;
    }

    fprintf(stdout, "Usage: %s [-a] [-c] [-q] [-h]\n", name);
    fprintf(stdout, "  -a    show all available fields\n");
    fprintf(stdout, "  -c    disable colour output\n");
    fprintf(stdout, "  -q    quiet mode (values only)\n");
    fprintf(stdout, "  -h    display this help\n");
}
