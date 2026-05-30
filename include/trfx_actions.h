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

typedef enum {
  TRFX_ACTION_RESULT_OK = 0,
  TRFX_ACTION_RESULT_CANCELLED,
  TRFX_ACTION_RESULT_PERMISSION_DENIED,
  TRFX_ACTION_RESULT_UNSUPPORTED,
  TRFX_ACTION_RESULT_INVALID,
  TRFX_ACTION_RESULT_NOT_FOUND,
  TRFX_ACTION_RESULT_FAILED
} TrfxActionResultStatus;

typedef struct {
  TrfxActionRequest request;
  TrfxActionResultStatus status;
  int system_errno;
  char message[256];
} TrfxActionResult;

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
const char *trfx_action_result_status_name(TrfxActionResultStatus status);
void trfx_init_action_result(TrfxActionResult *result);
int trfx_lookup_process_uid(const char *pid, unsigned int *uid,
                            char *error, size_t error_size);
int trfx_lookup_process_name(const char *pid, char *process,
                             size_t process_size, char *error,
                             size_t error_size);
TrfxActionResult trfx_execute_action_request(const TrfxActionRequest *request,
                                             int confirmed,
                                             unsigned int effective_uid,
                                             char *error,
                                             size_t error_size);

#endif // TRFX_ACTIONS_H
