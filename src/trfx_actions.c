/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_actions.h"

#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void set_string(char *dest, size_t dest_size, const char *value) {
  if (!dest || dest_size == 0)
    return;

  if (!value || value[0] == '\0') {
    snprintf(dest, dest_size, "-");
    return;
  }

  snprintf(dest, dest_size, "%s", value);
}

static int parse_pid_value(const char *pid, pid_t *value) {
  char *end = NULL;
  long parsed;

  if (!pid || pid[0] == '\0' || !value)
    return 0;

  errno = 0;
  parsed = strtol(pid, &end, 10);
  if (errno != 0 || !end || *end != '\0' || parsed <= 0)
    return 0;

  *value = (pid_t)parsed;
  return 1;
}

static int lookup_process_uid_file(FILE *fp, unsigned int *uid) {
  char line[256];

  if (!fp || !uid)
    return 0;

  while (fgets(line, sizeof(line), fp)) {
    unsigned int real_uid;

    if (sscanf(line, "Uid:%u", &real_uid) == 1) {
      *uid = real_uid;
      return 1;
    }
  }

  return 0;
}

static int split_endpoint(const char *endpoint, char *host, size_t host_size,
                          char *port, size_t port_size) {
  const char *colon;
  const char *end;
  size_t host_len;

  if (!endpoint || !host || !port || host_size == 0 || port_size == 0)
    return 0;

  if (endpoint[0] == '[') {
    end = strchr(endpoint, ']');
    if (!end || end[1] != ':')
      return 0;
    host_len = (size_t)(end - endpoint - 1);
    if (host_len == 0 || host_len >= host_size)
      return 0;
    memcpy(host, endpoint + 1, host_len);
    host[host_len] = '\0';
    snprintf(port, port_size, "%s", end + 2);
    return port[0] != '\0';
  }

  colon = strrchr(endpoint, ':');
  if (!colon)
    return 0;

  host_len = (size_t)(colon - endpoint);
  if (host_len == 0 || host_len >= host_size)
    return 0;
  memcpy(host, endpoint, host_len);
  host[host_len] = '\0';
  snprintf(port, port_size, "%s", colon + 1);
  return port[0] != '\0';
}

static pthread_mutex_t audit_mutex = PTHREAD_MUTEX_INITIALIZER;
static TrfxActionAuditEntry audit_entries[TRFX_ACTION_AUDIT_MAX];
static size_t audit_count = 0;
static size_t audit_next = 0;

static TrfxActionResult finalize_action_result(const TrfxActionRequest *request,
                                               TrfxActionResult result) {
  trfx_record_action_audit(request, &result);
  return result;
}

static int run_ss_drop_action(const TrfxActionRequest *request, char *error,
                              size_t error_size) {
  char local_host[64];
  char local_port[8];
  char remote_host[64];
  char remote_port[8];
  const char *argv[16];
  pid_t child;
  int status;
  int argc = 0;

  if (error && error_size > 0)
    error[0] = '\0';

  if (!request) {
    if (error && error_size > 0)
      snprintf(error, error_size, "invalid argument");
    return 0;
  }

  if (!split_endpoint(request->target.local, local_host, sizeof(local_host),
                      local_port, sizeof(local_port)) ||
      !split_endpoint(request->target.remote, remote_host,
                      sizeof(remote_host), remote_port, sizeof(remote_port))) {
    if (error && error_size > 0)
      snprintf(error, error_size, "connection endpoints unavailable");
    return 0;
  }

  argv[argc++] = "ss";
  argv[argc++] = "-K";
  if (strcmp(request->target.protocol, "UDP") == 0) {
    argv[argc++] = "udp";
  } else {
    argv[argc++] = "tcp";
  }
  if (local_host[0] != '\0' && local_port[0] != '\0') {
    argv[argc++] = "src";
    argv[argc++] = local_host;
    argv[argc++] = "sport";
    argv[argc++] = local_port;
  }
  if (remote_host[0] != '\0' && remote_port[0] != '\0') {
    argv[argc++] = "dst";
    argv[argc++] = remote_host;
    argv[argc++] = "dport";
    argv[argc++] = remote_port;
  }
  argv[argc] = NULL;

  child = fork();
  if (child == 0) {
    execvp("ss", (char *const *)argv);
    _exit(errno == ENOENT ? 127 : 126);
  }

  if (child < 0) {
    if (error && error_size > 0)
      snprintf(error, error_size, "failed to launch ss: %s", strerror(errno));
    return 0;
  }

  if (waitpid(child, &status, 0) < 0) {
    if (error && error_size > 0)
      snprintf(error, error_size, "failed to wait for ss: %s",
               strerror(errno));
    return 0;
  }

  if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
    return 1;

  if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
    if (error && error_size > 0)
      snprintf(error, error_size, "ss is not installed");
  } else if (error && error_size > 0) {
    snprintf(error, error_size, "ss could not drop the selected target");
  }

  return 0;
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

void trfx_init_action_result(TrfxActionResult *result) {
  if (!result)
    return;

  memset(result, 0, sizeof(*result));
  result->status = TRFX_ACTION_RESULT_INVALID;
  snprintf(result->message, sizeof(result->message), "no action selected");
}

void trfx_record_action_audit(const TrfxActionRequest *request,
                              const TrfxActionResult *result) {
  TrfxActionAuditEntry entry;

  memset(&entry, 0, sizeof(entry));
  entry.when = time(NULL);
  if (request) {
    entry.request = *request;
  } else {
    trfx_init_action_request(&entry.request);
  }

  if (result) {
    entry.status = result->status;
    if (result->message[0] != '\0') {
      snprintf(entry.message, sizeof(entry.message), "%s", result->message);
    } else {
      snprintf(entry.message, sizeof(entry.message), "%s",
               trfx_action_result_status_name(result->status));
    }
  } else {
    entry.status = TRFX_ACTION_RESULT_INVALID;
    snprintf(entry.message, sizeof(entry.message), "no result available");
  }

  pthread_mutex_lock(&audit_mutex);
  audit_entries[audit_next] = entry;
  audit_next = (audit_next + 1) % TRFX_ACTION_AUDIT_MAX;
  if (audit_count < TRFX_ACTION_AUDIT_MAX)
    audit_count++;
  pthread_mutex_unlock(&audit_mutex);
}

size_t trfx_action_audit_count(void) {
  size_t count;

  pthread_mutex_lock(&audit_mutex);
  count = audit_count;
  pthread_mutex_unlock(&audit_mutex);
  return count;
}

const TrfxActionAuditEntry *trfx_action_audit_at(size_t index) {
  const TrfxActionAuditEntry *entry = NULL;
  size_t pos;

  pthread_mutex_lock(&audit_mutex);
  if (index < audit_count) {
    pos = (audit_next + TRFX_ACTION_AUDIT_MAX - 1 - index) %
          TRFX_ACTION_AUDIT_MAX;
    entry = &audit_entries[pos];
  }
  pthread_mutex_unlock(&audit_mutex);
  return entry;
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

const char *trfx_action_result_status_name(TrfxActionResultStatus status) {
  switch (status) {
  case TRFX_ACTION_RESULT_OK:
    return "ok";
  case TRFX_ACTION_RESULT_CANCELLED:
    return "cancelled";
  case TRFX_ACTION_RESULT_PERMISSION_DENIED:
    return "permission denied";
  case TRFX_ACTION_RESULT_UNSUPPORTED:
    return "unsupported";
  case TRFX_ACTION_RESULT_NOT_FOUND:
    return "not found";
  case TRFX_ACTION_RESULT_FAILED:
    return "failed";
  case TRFX_ACTION_RESULT_INVALID:
  default:
    return "invalid";
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

int trfx_lookup_process_uid(const char *pid, unsigned int *uid, char *error,
                            size_t error_size) {
  char path[256];
  FILE *fp;

  if (error && error_size > 0)
    error[0] = '\0';

  if (!pid || !uid) {
    if (error && error_size > 0)
      snprintf(error, error_size, "invalid argument");
    return 0;
  }

  snprintf(path, sizeof(path), "/proc/%s/status", pid);
  fp = fopen(path, "r");
  if (!fp) {
    if (error && error_size > 0)
      snprintf(error, error_size, "process %s not found", pid);
    return 0;
  }

  if (!lookup_process_uid_file(fp, uid)) {
    fclose(fp);
    if (error && error_size > 0)
      snprintf(error, error_size, "uid unavailable for process %s", pid);
    return 0;
  }

  fclose(fp);
  return 1;
}

int trfx_lookup_process_name(const char *pid, char *process, size_t process_size,
                             char *error, size_t error_size) {
  char path[256];
  FILE *fp;

  if (error && error_size > 0)
    error[0] = '\0';

  if (!pid || !process || process_size == 0) {
    if (error && error_size > 0)
      snprintf(error, error_size, "invalid argument");
    return 0;
  }

  snprintf(path, sizeof(path), "/proc/%s/comm", pid);
  fp = fopen(path, "r");
  if (!fp) {
    if (error && error_size > 0)
      snprintf(error, error_size, "process %s not found", pid);
    return 0;
  }

  if (!fgets(process, (int)process_size, fp)) {
    fclose(fp);
    if (error && error_size > 0)
      snprintf(error, error_size, "process name unavailable for %s", pid);
    return 0;
  }

  fclose(fp);
  process[strcspn(process, "\n")] = '\0';
  if (process[0] == '\0')
    snprintf(process, process_size, "-");
  return 1;
}

TrfxActionResult trfx_execute_action_request(const TrfxActionRequest *request,
                                             int confirmed,
                                             unsigned int effective_uid,
                                             char *error,
                                             size_t error_size) {
  TrfxActionResult result;
  unsigned int target_uid = 0;
  pid_t pid_value;

  if (error && error_size > 0)
    error[0] = '\0';

  trfx_init_action_result(&result);
  if (request)
    result.request = *request;

  if (!request || request->kind == TRFX_ACTION_KIND_NONE) {
    result.status = TRFX_ACTION_RESULT_INVALID;
    snprintf(result.message, sizeof(result.message), "no action selected");
    if (error && error_size > 0)
      snprintf(error, error_size, "%s", result.message);
    return finalize_action_result(request, result);
  }

  if (!confirmed) {
    result.status = TRFX_ACTION_RESULT_CANCELLED;
    snprintf(result.message, sizeof(result.message), "action cancelled");
    if (error && error_size > 0)
      snprintf(error, error_size, "%s", result.message);
    return finalize_action_result(request, result);
  }

  if (request->kind == TRFX_ACTION_KIND_KILL_PROCESS) {
    if (!parse_pid_value(request->target.pid, &pid_value)) {
      result.status = TRFX_ACTION_RESULT_INVALID;
      snprintf(result.message, sizeof(result.message),
               "invalid process id: %s",
               request->target.pid[0] ? request->target.pid : "-");
      if (error && error_size > 0)
        snprintf(error, error_size, "%s", result.message);
      return finalize_action_result(request, result);
    }

    if (!trfx_lookup_process_uid(request->target.pid, &target_uid, error,
                                 error_size)) {
      result.status = TRFX_ACTION_RESULT_NOT_FOUND;
      snprintf(result.message, sizeof(result.message), "process %s not found",
               request->target.pid);
      if (error && error_size > 0 && error[0] == '\0')
        snprintf(error, error_size, "%s", result.message);
      return finalize_action_result(request, result);
    }

    if (effective_uid != 0 && effective_uid != target_uid) {
      result.status = TRFX_ACTION_RESULT_PERMISSION_DENIED;
      snprintf(result.message, sizeof(result.message),
               "permission denied: requires root or uid %u", target_uid);
      if (error && error_size > 0)
        snprintf(error, error_size, "%s", result.message);
      return finalize_action_result(request, result);
    }

    if (kill(pid_value, SIGTERM) != 0) {
      result.system_errno = errno;
      if (errno == ESRCH) {
        result.status = TRFX_ACTION_RESULT_NOT_FOUND;
        snprintf(result.message, sizeof(result.message),
                 "process %s not found", request->target.pid);
      } else if (errno == EPERM) {
        result.status = TRFX_ACTION_RESULT_PERMISSION_DENIED;
        snprintf(result.message, sizeof(result.message),
                 "permission denied while killing %s", request->target.pid);
      } else {
        result.status = TRFX_ACTION_RESULT_FAILED;
        snprintf(result.message, sizeof(result.message),
                 "failed to kill %s: %s", request->target.pid,
                 strerror(errno));
      }
      if (error && error_size > 0)
        snprintf(error, error_size, "%s", result.message);
      return finalize_action_result(request, result);
    }

    result.status = TRFX_ACTION_RESULT_OK;
    snprintf(result.message, sizeof(result.message), "sent SIGTERM to %s",
             request->target.pid);
    if (error && error_size > 0)
      snprintf(error, error_size, "%s", result.message);
    return finalize_action_result(request, result);
  }

  if (request->kind == TRFX_ACTION_KIND_DROP_CONNECTION ||
      request->kind == TRFX_ACTION_KIND_DROP_SOCKET) {
    if (effective_uid != 0) {
      result.status = TRFX_ACTION_RESULT_PERMISSION_DENIED;
      snprintf(result.message, sizeof(result.message),
               "permission denied: root required to drop sockets");
      if (error && error_size > 0)
        snprintf(error, error_size, "%s", result.message);
      return finalize_action_result(request, result);
    }

    if (!run_ss_drop_action(request, error, error_size)) {
      if (error && error_size > 0 && error[0] != '\0') {
        if (strstr(error, "not installed") != NULL) {
          result.status = TRFX_ACTION_RESULT_UNSUPPORTED;
        } else {
          result.status = TRFX_ACTION_RESULT_FAILED;
        }
        snprintf(result.message, sizeof(result.message), "%s", error);
      } else {
        result.status = TRFX_ACTION_RESULT_UNSUPPORTED;
        snprintf(result.message, sizeof(result.message),
                 "%s is not supported yet",
                 trfx_action_kind_name(request->kind));
      }
      return finalize_action_result(request, result);
    }

    result.status = TRFX_ACTION_RESULT_OK;
    snprintf(result.message, sizeof(result.message), "%s completed",
             trfx_action_kind_name(request->kind));
    if (error && error_size > 0)
      snprintf(error, error_size, "%s", result.message);
    return finalize_action_result(request, result);
  }

  result.status = TRFX_ACTION_RESULT_UNSUPPORTED;
  snprintf(result.message, sizeof(result.message), "%s is not supported yet",
           trfx_action_kind_name(request->kind));
  if (error && error_size > 0)
    snprintf(error, error_size, "%s", result.message);
  return finalize_action_result(request, result);
}
