/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_actions.h"

#include <stdio.h>
#include <string.h>

static void set_string(char *dest, size_t dest_size, const char *value) {
  if (!dest || dest_size == 0)
    return;

  if (!value || value[0] == '\0') {
    snprintf(dest, dest_size, "-");
    return;
  }

  snprintf(dest, dest_size, "%s", value);
}

void trfx_init_action_request(TrfxActionRequest *request) {
  if (!request)
    return;

  memset(request, 0, sizeof(*request));
  request->kind = TRFX_ACTION_KIND_NONE;
  request->target.kind = TRFX_ACTION_TARGET_NONE;
  request->requires_confirmation = 1;
  request->requires_permission_check = 1;
  snprintf(request->label, sizeof(request->label), "no action");
  snprintf(request->description, sizeof(request->description),
           "no action selected");
}

void trfx_init_action_review(TrfxActionReview *review) {
  if (!review)
    return;

  memset(review, 0, sizeof(*review));
  review->permission = TRFX_ACTION_PERMISSION_UNKNOWN;
  snprintf(review->prompt, sizeof(review->prompt), "no action selected");
  snprintf(review->details, sizeof(review->details), "no action selected");
}

const char *trfx_action_kind_name(TrfxActionKind kind) {
  switch (kind) {
  case TRFX_ACTION_KIND_KILL_PROCESS:
    return "kill process";
  case TRFX_ACTION_KIND_DROP_CONNECTION:
    return "drop connection";
  case TRFX_ACTION_KIND_DROP_SOCKET:
    return "drop socket";
  case TRFX_ACTION_KIND_NONE:
  default:
    return "none";
  }
}

const char *trfx_action_target_kind_name(TrfxActionTargetKind kind) {
  switch (kind) {
  case TRFX_ACTION_TARGET_PROCESS:
    return "process";
  case TRFX_ACTION_TARGET_CONNECTION:
    return "connection";
  case TRFX_ACTION_TARGET_SOCKET:
    return "socket";
  case TRFX_ACTION_TARGET_NONE:
  default:
    return "none";
  }
}

const char *trfx_action_permission_status_name(
    TrfxActionPermissionStatus status) {
  switch (status) {
  case TRFX_ACTION_PERMISSION_ALLOWED:
    return "allowed";
  case TRFX_ACTION_PERMISSION_DENIED:
    return "permission denied";
  case TRFX_ACTION_PERMISSION_UNSUPPORTED:
    return "unsupported";
  case TRFX_ACTION_PERMISSION_UNKNOWN:
  default:
    return "unknown";
  }
}

void trfx_action_request_set_process_kill(TrfxActionRequest *request,
                                          const char *pid,
                                          const char *process) {
  if (!request)
    return;

  trfx_init_action_request(request);
  request->kind = TRFX_ACTION_KIND_KILL_PROCESS;
  request->target.kind = TRFX_ACTION_TARGET_PROCESS;
  set_string(request->target.pid, sizeof(request->target.pid), pid);
  set_string(request->target.process, sizeof(request->target.process),
             process);
  snprintf(request->label, sizeof(request->label), "kill process %s",
           request->target.pid);
  snprintf(request->description, sizeof(request->description),
           "kill process %s (%s)", request->target.pid,
           request->target.process);
}

void trfx_action_request_set_connection_drop(TrfxActionRequest *request,
                                             const ConnectionInfo *connection) {
  if (!request)
    return;

  trfx_init_action_request(request);
  request->kind = TRFX_ACTION_KIND_DROP_CONNECTION;
  request->target.kind = TRFX_ACTION_TARGET_CONNECTION;

  if (connection) {
    set_string(request->target.protocol, sizeof(request->target.protocol),
               connection->protocol);
    set_string(request->target.local, sizeof(request->target.local),
               connection->local_addr);
    set_string(request->target.remote, sizeof(request->target.remote),
               connection->remote_addr);
    set_string(request->target.pid, sizeof(request->target.pid),
               connection->pid);
    set_string(request->target.process, sizeof(request->target.process),
               connection->process);
    set_string(request->target.inode, sizeof(request->target.inode), "-");
    snprintf(request->label, sizeof(request->label), "drop connection %s",
             request->target.remote);
    snprintf(request->description, sizeof(request->description),
             "drop %s %s -> %s (%s/%s)", request->target.protocol,
             request->target.local, request->target.remote,
             request->target.pid, request->target.process);
  } else {
    snprintf(request->label, sizeof(request->label), "drop connection");
    snprintf(request->description, sizeof(request->description),
             "drop selected connection");
  }
}

void trfx_action_request_set_socket_drop(TrfxActionRequest *request,
                                         const SocketOwnerInfo *socket_owner) {
  if (!request)
    return;

  trfx_init_action_request(request);
  request->kind = TRFX_ACTION_KIND_DROP_SOCKET;
  request->target.kind = TRFX_ACTION_TARGET_SOCKET;

  if (socket_owner) {
    set_string(request->target.protocol, sizeof(request->target.protocol),
               socket_owner->proto);
    set_string(request->target.local, sizeof(request->target.local),
               socket_owner->laddr);
    set_string(request->target.remote, sizeof(request->target.remote),
               socket_owner->raddr);
    set_string(request->target.pid, sizeof(request->target.pid),
               socket_owner->pid);
    set_string(request->target.process, sizeof(request->target.process),
               socket_owner->process);
    snprintf(request->label, sizeof(request->label), "drop socket %s",
             request->target.local);
    snprintf(request->description, sizeof(request->description),
             "drop %s socket %s -> %s (%s/%s)", request->target.protocol,
             request->target.local, request->target.remote,
             request->target.pid, request->target.process);
  } else {
    snprintf(request->label, sizeof(request->label), "drop socket");
    snprintf(request->description, sizeof(request->description),
             "drop selected socket");
  }
}

void trfx_prepare_action_review(TrfxActionReview *review,
                                const TrfxActionRequest *request,
                                unsigned int effective_uid,
                                unsigned int target_uid, int supported) {
  if (!review)
    return;

  trfx_init_action_review(review);

  if (!request)
    return;

  review->request = *request;
  review->requires_confirmation = request->requires_confirmation;
  snprintf(review->details, sizeof(review->details), "%s",
           request->description);

  if (!supported) {
    review->permission = TRFX_ACTION_PERMISSION_UNSUPPORTED;
    review->can_execute = 0;
    snprintf(review->prompt, sizeof(review->prompt),
             "%s is not supported on this system",
             trfx_action_kind_name(request->kind));
    return;
  }

  if (request->requires_permission_check && effective_uid != 0 &&
      effective_uid != target_uid) {
    review->permission = TRFX_ACTION_PERMISSION_DENIED;
    review->can_execute = 0;
    snprintf(review->prompt, sizeof(review->prompt),
             "permission denied: requires root or uid %u", target_uid);
    return;
  }

  review->permission = TRFX_ACTION_PERMISSION_ALLOWED;
  review->can_execute = 1;
  snprintf(review->prompt, sizeof(review->prompt), "confirm %.240s?",
           request->description);
}
