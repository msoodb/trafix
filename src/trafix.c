/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_app.h"
#include "trfx_cli.h"

int main(int argc, char **argv) {
    TrfxCliOptions options = trfx_parse_cli(argc, argv);

    switch (options.mode) {
    case TRFX_CLI_MODE_HELP:
        trfx_print_cli_help();
        return 0;
    case TRFX_CLI_MODE_VERSION:
        trfx_print_cli_version();
        return 0;
    case TRFX_CLI_MODE_INVALID:
        trfx_print_cli_error(&options);
        return 1;
    case TRFX_CLI_MODE_TUI:
        break;
    }

    return trfx_run_tui();
}
