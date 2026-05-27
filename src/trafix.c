/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include <stdio.h>
#include "trfx_app.h"
#include "trfx_cli.h"
#include "trfx_version.h"

static void print_version(void) {
    printf("trafix %s\n", trfx_get_version());
}

int main(int argc, char **argv) {
    TrfxCliOptions options = trfx_parse_cli(argc, argv);

    switch (options.mode) {
    case TRFX_CLI_MODE_HELP:
        trfx_print_cli_help();
        return 0;
    case TRFX_CLI_MODE_VERSION:
        print_version();
        return 0;
    case TRFX_CLI_MODE_INVALID:
        fprintf(stderr, "trafix: %s\n", options.error);
        fprintf(stderr, "Try 'trafix --help' for usage.\n");
        return 1;
    case TRFX_CLI_MODE_TUI:
        break;
    }

    return trfx_run_tui();
}
