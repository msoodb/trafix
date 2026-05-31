/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_app.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "trfx_actions.h"
#include "trfx_diagnostics.h"
#include "trfx_cli_output.h"
#include "trfx_config.h"
#include "trfx_connections.h"
#include "trfx_dashboard.h"
#include "trfx_procinfo.h"
#include "trfx_socket_owners.h"
#include "trfx_netinfo.h"
#include "trfx_sysinfo.h"

int trfx_run_tui(const TrfxCliOptions *options) {
  char error[256];
  srand(time(NULL));
  if (!trfx_load_runtime_config(options && options->has_profile
                                    ? options->profile_name
                                    : NULL,
                                error, sizeof(error))) {
    fprintf(stderr, "trafix: %s\n", error[0] ? error : "failed to load profile");
    return TRFX_EXIT_ERROR;
  }
  ROW2_MODULES = 1;
  start_dashboard();
  return TRFX_EXIT_OK;
}

int trfx_run_interfaces_command(TrfxCliOutputFormat output_format) {
  TrfxInterfaceStatsResult result =
      trfx_collect_interface_stats_path("/proc/net/dev");

  if (result.status != TRFX_COLLECTOR_OK) {
    fprintf(stderr, "trafix: failed to collect interfaces: %s\n",
            result.error[0] ? result.error : "unknown error");
    return TRFX_EXIT_DATA_UNAVAILABLE;
  }

  if (output_format == TRFX_CLI_OUTPUT_JSON) {
    trfx_print_interfaces_json(stdout, &result);
    return TRFX_EXIT_OK;
  }

  trfx_print_interfaces_text(stdout, &result);

  return TRFX_EXIT_OK;
}

int trfx_run_connections_command(const TrfxCliOptions *options) {
  ConnectionInfo connections[MAX_CONNECTIONS];
  int count = get_connection_info(connections, MAX_CONNECTIONS);
  TrfxCliOutputFormat output_format =
      options ? options->output_format : TRFX_CLI_OUTPUT_TEXT;

  if (output_format == TRFX_CLI_OUTPUT_JSON) {
    trfx_print_connections_json(stdout, connections, count, options);
    return TRFX_EXIT_OK;
  }

  trfx_print_connections_text(stdout, connections, count, options);

  return TRFX_EXIT_OK;
}

int trfx_run_listeners_command(TrfxCliOutputFormat output_format) {
  ConnectionInfo connections[MAX_CONNECTIONS];
  int count = get_connection_info(connections, MAX_CONNECTIONS);

  if (output_format == TRFX_CLI_OUTPUT_JSON) {
    trfx_print_listeners_json(stdout, connections, count);
    return TRFX_EXIT_OK;
  }

  trfx_print_listeners_text(stdout, connections, count);
  return TRFX_EXIT_OK;
}

int trfx_run_system_command(TrfxCliOutputFormat output_format) {
  SystemOverview overview = get_system_overview();

  if (output_format == TRFX_CLI_OUTPUT_JSON) {
    trfx_print_system_json(stdout, &overview);
    return TRFX_EXIT_OK;
  }

  trfx_print_system_text(stdout, &overview);

  return TRFX_EXIT_OK;
}

int trfx_run_diagnostics_command(const TrfxCliOptions *options) {
  TrfxDiagnosticsSnapshot snapshot;
  char error[256];
  TrfxCollectorStatus status;

  if (!trfx_load_runtime_config(options && options->has_profile
                                    ? options->profile_name
                                    : NULL,
                                error, sizeof(error))) {
    fprintf(stderr, "trafix: %s\n", error[0] ? error : "failed to load profile");
    return TRFX_EXIT_ERROR;
  }

  status = trfx_collect_diagnostics_snapshot(&snapshot, error, sizeof(error));
  if (status != TRFX_COLLECTOR_OK && error[0] != '\0') {
    fprintf(stderr, "trafix: %s\n", error);
  }

  trfx_print_diagnostics_text(stdout, &snapshot);

  if (status == TRFX_COLLECTOR_OK || status == TRFX_COLLECTOR_OPEN_FAILED ||
      status == TRFX_COLLECTOR_PARSE_FAILED)
    return TRFX_EXIT_OK;
  return TRFX_EXIT_ERROR;
}

static int prompt_confirm_action(const TrfxActionReview *review) {
  int ch;

  if (!review)
    return 0;

  fprintf(stderr, "%s\n", review->prompt);
  fprintf(stderr, "%s\n", review->details);
  fprintf(stderr, "Proceed? [y/N] ");
  fflush(stderr);

  ch = getchar();
  fprintf(stderr, "\n");
  return ch == 'y' || ch == 'Y';
}

static int connection_matches_drop_target(const ConnectionInfo *connection,
                                         const TrfxCliOptions *options) {
  if (!connection || !options)
    return 0;

  return strcmp(connection->protocol, options->drop_proto) == 0 &&
         strcmp(connection->local_addr, options->drop_local) == 0 &&
         strcmp(connection->remote_addr, options->drop_remote) == 0;
}

static int socket_matches_drop_target(const SocketOwnerInfo *socket_owner,
                                      const TrfxCliOptions *options) {
  if (!socket_owner || !options)
    return 0;

  return strcmp(socket_owner->proto, options->drop_proto) == 0 &&
         strcmp(socket_owner->laddr, options->drop_local) == 0 &&
         strcmp(socket_owner->raddr, options->drop_remote) == 0;
}

int trfx_run_kill_command(const TrfxCliOptions *options) {
  TrfxActionRequest request;
  TrfxActionReview review;
  TrfxActionResult result;
  char process_name[64];
  char error[256];
  unsigned int target_uid = 0;
  int confirmed;

  if (!options || !options->has_target_pid) {
    fprintf(stderr, "trafix: kill requires a PID\n");
    return TRFX_EXIT_ERROR;
  }

  trfx_action_request_set_process_kill(&request, options->target_pid, "-");
  if (trfx_lookup_process_name(options->target_pid, process_name,
                               sizeof(process_name), error,
                               sizeof(error))) {
    snprintf(request.target.process, sizeof(request.target.process), "%s",
             process_name);
    snprintf(request.description, sizeof(request.description),
             "kill process %s (%s)", request.target.pid,
             request.target.process);
    snprintf(request.label, sizeof(request.label), "kill process %s",
             request.target.pid);
  }

  if (!trfx_lookup_process_uid(options->target_pid, &target_uid, error,
                               sizeof(error))) {
    fprintf(stderr, "trafix: %s\n", error[0] ? error : "process not found");
    return TRFX_EXIT_DATA_UNAVAILABLE;
  }

  trfx_prepare_action_review(&review, &request, (unsigned int)geteuid(),
                             target_uid, 1);

  confirmed = options->confirmed;
  if (!confirmed) {
    if (!isatty(STDIN_FILENO)) {
      fprintf(stderr, "trafix: confirmation required for kill command\n");
      return TRFX_EXIT_ERROR;
    }
    confirmed = prompt_confirm_action(&review);
  }

  result = trfx_execute_action_request(&request, confirmed,
                                       (unsigned int)geteuid(), error,
                                       sizeof(error));

  fprintf(stderr, "trafix: %s\n",
          result.message[0] ? result.message : "action completed");

  if (result.status == TRFX_ACTION_RESULT_OK)
    return TRFX_EXIT_OK;
  if (result.status == TRFX_ACTION_RESULT_NOT_FOUND)
    return TRFX_EXIT_DATA_UNAVAILABLE;
  return TRFX_EXIT_ERROR;
}

int trfx_run_drop_command(const TrfxCliOptions *options) {
  TrfxActionRequest request;
  TrfxActionReview review;
  TrfxActionResult result;
  char error[256];
  int confirmed;
  int found = 0;

  if (!options || !options->has_drop_target) {
    fprintf(stderr, "trafix: drop requires a target type and endpoints\n");
    return TRFX_EXIT_ERROR;
  }

  trfx_init_action_request(&request);
  if (strcmp(options->drop_kind, "connection") == 0) {
    ConnectionInfo connections[MAX_CONNECTIONS];
    int count = get_connection_info(connections, MAX_CONNECTIONS);

    for (int i = 0; i < count; i++) {
      if (connection_matches_drop_target(&connections[i], options)) {
        trfx_action_request_set_connection_drop(&request, &connections[i]);
        found = 1;
        break;
      }
    }
  } else if (strcmp(options->drop_kind, "socket") == 0) {
    SocketOwnerInfo sockets[MAX_SOCKET_OWNERS];
    int count = get_socket_owner_info(sockets, MAX_SOCKET_OWNERS);

    for (int i = 0; i < count; i++) {
      if (socket_matches_drop_target(&sockets[i], options)) {
        trfx_action_request_set_socket_drop(&request, &sockets[i]);
        found = 1;
        break;
      }
    }
  }

  if (!found) {
    fprintf(stderr, "trafix: target not found\n");
    return TRFX_EXIT_DATA_UNAVAILABLE;
  }

  trfx_prepare_action_review(&review, &request, (unsigned int)geteuid(), 0, 1);

  confirmed = options->confirmed;
  if (!confirmed) {
    if (!isatty(STDIN_FILENO)) {
      fprintf(stderr, "trafix: confirmation required for drop command\n");
      return TRFX_EXIT_ERROR;
    }
    confirmed = prompt_confirm_action(&review);
  }

  result = trfx_execute_action_request(&request, confirmed,
                                       (unsigned int)geteuid(), error,
                                       sizeof(error));

  fprintf(stderr, "trafix: %s\n",
          result.message[0] ? result.message : "action completed");

  if (result.status == TRFX_ACTION_RESULT_OK)
    return TRFX_EXIT_OK;
  if (result.status == TRFX_ACTION_RESULT_NOT_FOUND)
    return TRFX_EXIT_DATA_UNAVAILABLE;
  return TRFX_EXIT_ERROR;
}
