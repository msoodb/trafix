/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#ifndef TRFX_ACTIONS_H
#define TRFX_ACTIONS_H

#include "trfx_connections.h"
#include "trfx_socket_owners.h"

typedef enum {
  TRFX_ACTION_KIND_NONE = 0,
  TRFX_ACTION_KIND_KILL_PROCESS,
  TRFX_ACTION_KIND_DROP_CONNECTION,
  TRFX_ACTION_KIND_DROP_SOCKET
} TrfxActionKind;

typedef enum {
  TRFX_ACTION_TARGET_NONE = 0,
  TRFX_ACTION_TARGET_PROCESS,
  TRFX_ACTION_TARGET_CONNECTION,
  TRFX_ACTION_TARGET_SOCKET
} TrfxActionTargetKind;

typedef struct {
  TrfxActionTargetKind kind;
  char pid[16];
  char process[64];
  char protocol[8];
  char local[64];
  char remote[64];
  char inode[16];
} TrfxActionTarget;

typedef struct {
  TrfxActionKind kind;
  TrfxActionTarget target;
  int requires_confirmation;
  int requires_permission_check;
  char label[128];
  char description[256];
} TrfxActionRequest;

void trfx_init_action_request(TrfxActionRequest *request);
void trfx_action_request_set_process_kill(TrfxActionRequest *request,
                                          const char *pid,
                                          const char *process);
void trfx_action_request_set_connection_drop(TrfxActionRequest *request,
                                             const ConnectionInfo *connection);
void trfx_action_request_set_socket_drop(TrfxActionRequest *request,
                                         const SocketOwnerInfo *socket_owner);
const char *trfx_action_kind_name(TrfxActionKind kind);
const char *trfx_action_target_kind_name(TrfxActionTargetKind kind);

#endif // TRFX_ACTIONS_H
