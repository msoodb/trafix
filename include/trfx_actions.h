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
  TRFX_ACTION_PERMISSION_UNKNOWN = 0,
  TRFX_ACTION_PERMISSION_ALLOWED,
  TRFX_ACTION_PERMISSION_DENIED,
  TRFX_ACTION_PERMISSION_UNSUPPORTED
} TrfxActionPermissionStatus;

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

typedef struct {
  TrfxActionRequest request;
  TrfxActionPermissionStatus permission;
  int can_execute;
  int requires_confirmation;
  char prompt[256];
  char details[256];
} TrfxActionReview;

void trfx_init_action_request(TrfxActionRequest *request);
void trfx_action_request_set_process_kill(TrfxActionRequest *request,
                                          const char *pid,
                                          const char *process);
void trfx_action_request_set_connection_drop(TrfxActionRequest *request,
                                             const ConnectionInfo *connection);
void trfx_action_request_set_socket_drop(TrfxActionRequest *request,
                                         const SocketOwnerInfo *socket_owner);
void trfx_init_action_review(TrfxActionReview *review);
void trfx_prepare_action_review(TrfxActionReview *review,
                                const TrfxActionRequest *request,
                                unsigned int effective_uid,
                                unsigned int target_uid, int supported);
const char *trfx_action_kind_name(TrfxActionKind kind);
const char *trfx_action_target_kind_name(TrfxActionTargetKind kind);
const char *trfx_action_permission_status_name(
    TrfxActionPermissionStatus status);

#endif // TRFX_ACTIONS_H
