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
        return TRFX_EXIT_OK;
    case TRFX_CLI_MODE_VERSION:
        trfx_print_cli_version();
        return TRFX_EXIT_OK;
    case TRFX_CLI_MODE_INVALID:
        trfx_print_cli_error(&options);
        return TRFX_EXIT_ERROR;
    case TRFX_CLI_MODE_INTERFACES:
        return trfx_run_interfaces_command(options.output_format);
    case TRFX_CLI_MODE_CONNECTIONS:
        return trfx_run_connections_command(&options);
    case TRFX_CLI_MODE_LISTENERS:
        return trfx_run_listeners_command(options.output_format);
    case TRFX_CLI_MODE_SYSTEM:
        return trfx_run_system_command(options.output_format);
    case TRFX_CLI_MODE_DIAGNOSTICS:
        return trfx_run_diagnostics_command();
    case TRFX_CLI_MODE_DROP:
        return trfx_run_drop_command(&options);
    case TRFX_CLI_MODE_KILL:
        return trfx_run_kill_command(&options);
    case TRFX_CLI_MODE_TUI:
        break;
    }

    return trfx_run_tui();
}
