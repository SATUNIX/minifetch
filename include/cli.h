#ifndef MINIFETCH_CLI_H
#define MINIFETCH_CLI_H

struct mf_options {
    int show_all;
    int no_colour;
    int quiet;
    int help;
};

int mf_cli_parse(int argc, char **argv, struct mf_options *opts);
void mf_cli_print_usage(const char *prog);

#endif /* MINIFETCH_CLI_H */
