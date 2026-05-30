/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "test_common.h"

#include "trfx_actions.h"

static int test_action_request_init(void) {
  TrfxActionRequest request;

  trfx_init_action_request(&request);
  ASSERT_INT_EQ(request.kind, TRFX_ACTION_KIND_NONE);
  ASSERT_INT_EQ(request.target.kind, TRFX_ACTION_TARGET_NONE);
  ASSERT_INT_EQ(request.requires_confirmation, 1);
  ASSERT_INT_EQ(request.requires_permission_check, 1);
  ASSERT_STR_EQ(request.label, "no action");
  ASSERT_STR_EQ(request.description, "no action selected");

  return 0;
}

static int test_action_request_process(void) {
  TrfxActionRequest request;

  trfx_action_request_set_process_kill(&request, "1234", "sshd");
  ASSERT_INT_EQ(request.kind, TRFX_ACTION_KIND_KILL_PROCESS);
  ASSERT_INT_EQ(request.target.kind, TRFX_ACTION_TARGET_PROCESS);
  ASSERT_STR_EQ(request.target.pid, "1234");
  ASSERT_STR_EQ(request.target.process, "sshd");
  ASSERT_STR_EQ(request.label, "kill process 1234");
  ASSERT_STR_EQ(request.description, "kill process 1234 (sshd)");

  return 0;
}

static int test_action_request_connection(void) {
  TrfxActionRequest request;
  ConnectionInfo connection = {0};

  snprintf(connection.protocol, sizeof(connection.protocol), "TCP");
  snprintf(connection.local_addr, sizeof(connection.local_addr),
           "127.0.0.1:8080");
  snprintf(connection.remote_addr, sizeof(connection.remote_addr),
           "10.0.0.2:443");
  snprintf(connection.pid, sizeof(connection.pid), "77");
  snprintf(connection.process, sizeof(connection.process), "curl");

  trfx_action_request_set_connection_drop(&request, &connection);
  ASSERT_INT_EQ(request.kind, TRFX_ACTION_KIND_DROP_CONNECTION);
  ASSERT_INT_EQ(request.target.kind, TRFX_ACTION_TARGET_CONNECTION);
  ASSERT_STR_EQ(request.target.protocol, "TCP");
  ASSERT_STR_EQ(request.target.local, "127.0.0.1:8080");
  ASSERT_STR_EQ(request.target.remote, "10.0.0.2:443");
  ASSERT_STR_EQ(request.target.pid, "77");
  ASSERT_STR_EQ(request.target.process, "curl");
  ASSERT_STR_EQ(request.label, "drop connection 10.0.0.2:443");

  return 0;
}

static int test_action_request_socket(void) {
  TrfxActionRequest request;
  SocketOwnerInfo socket_owner = {0};

  snprintf(socket_owner.proto, sizeof(socket_owner.proto), "UDP");
  snprintf(socket_owner.laddr, sizeof(socket_owner.laddr), "0.0.0.0");
  snprintf(socket_owner.raddr, sizeof(socket_owner.raddr), "0.0.0.0");
  snprintf(socket_owner.pid, sizeof(socket_owner.pid), "88");
  snprintf(socket_owner.process, sizeof(socket_owner.process), "dnsmasq");

  trfx_action_request_set_socket_drop(&request, &socket_owner);
  ASSERT_INT_EQ(request.kind, TRFX_ACTION_KIND_DROP_SOCKET);
  ASSERT_INT_EQ(request.target.kind, TRFX_ACTION_TARGET_SOCKET);
  ASSERT_STR_EQ(request.target.protocol, "UDP");
  ASSERT_STR_EQ(request.target.local, "0.0.0.0");
  ASSERT_STR_EQ(request.target.remote, "0.0.0.0");
  ASSERT_STR_EQ(request.target.pid, "88");
  ASSERT_STR_EQ(request.target.process, "dnsmasq");
  ASSERT_STR_EQ(request.label, "drop socket 0.0.0.0");

  return 0;
}

static int test_action_review_permissions(void) {
  TrfxActionRequest request;
  TrfxActionReview review;

  trfx_action_request_set_process_kill(&request, "1234", "sshd");

  trfx_init_action_review(&review);
  trfx_prepare_action_review(&review, &request, 1000, 1000, 1);
  ASSERT_INT_EQ(review.permission, TRFX_ACTION_PERMISSION_ALLOWED);
  ASSERT_INT_EQ(review.can_execute, 1);
  ASSERT_INT_EQ(review.requires_confirmation, 1);
  ASSERT_STR_EQ(review.prompt, "confirm kill process 1234 (sshd)?");
  ASSERT_STR_EQ(review.details, "kill process 1234 (sshd)");

  trfx_prepare_action_review(&review, &request, 1000, 1001, 1);
  ASSERT_INT_EQ(review.permission, TRFX_ACTION_PERMISSION_DENIED);
  ASSERT_INT_EQ(review.can_execute, 0);
  ASSERT_STR_EQ(review.prompt, "permission denied: requires root or uid 1001");

  trfx_prepare_action_review(&review, &request, 1000, 1000, 0);
  ASSERT_INT_EQ(review.permission, TRFX_ACTION_PERMISSION_UNSUPPORTED);
  ASSERT_INT_EQ(review.can_execute, 0);
  ASSERT_STR_EQ(review.prompt, "kill process is not supported on this system");

  return 0;
}

int main(void) {
  if (test_action_request_init() != 0)
    return 1;

  if (test_action_request_process() != 0)
    return 1;

  if (test_action_request_connection() != 0)
    return 1;

  if (test_action_request_socket() != 0)
    return 1;

  if (test_action_review_permissions() != 0)
    return 1;

  return 0;
}
