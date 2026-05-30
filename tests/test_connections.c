/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "test_common.h"
#include "trfx_connections.h"
#include "trfx_socket_owners.h"

static int test_parse_connection_fixtures(void) {
  ConnectionInfo connections[MAX_CONNECTIONS];

  int count = trfx_parse_connection_path("tests/fixtures/proc_net_tcp", "TCP",
                                         connections, 0, MAX_CONNECTIONS);
  ASSERT_INT_EQ(count, 2);
  ASSERT_STR_EQ(connections[0].protocol, "TCP");
  ASSERT_STR_EQ(connections[0].local_addr, "127.0.0.1:8080");
  ASSERT_STR_EQ(connections[0].remote_addr, "127.0.0.2:443");
  ASSERT_STR_EQ(connections[0].state, "ESTABLISHED");
  ASSERT_INT_EQ((int)connections[0].inode, 12345);
  ASSERT_INT_EQ((int)connections[0].uid, 1000);
  ASSERT_STR_EQ(connections[0].pid, "-");
  ASSERT_STR_EQ(connections[0].process, "-");
  ASSERT_STR_EQ(connections[1].protocol, "TCP");
  ASSERT_STR_EQ(connections[1].local_addr, "0.0.0.0:22");
  ASSERT_STR_EQ(connections[1].remote_addr, "0.0.0.0:0");
  ASSERT_STR_EQ(connections[1].state, "LISTEN");
  ASSERT_INT_EQ((int)connections[1].uid, 0);

  count = trfx_parse_connection_path("tests/fixtures/proc_net_udp", "UDP",
                                     connections, count, MAX_CONNECTIONS);
  ASSERT_INT_EQ(count, 4);
  ASSERT_STR_EQ(connections[2].protocol, "UDP");
  ASSERT_STR_EQ(connections[2].local_addr, "127.0.0.1:53");
  ASSERT_STR_EQ(connections[2].state, "UNCONN");
  ASSERT_STR_EQ(connections[3].protocol, "UDP");
  ASSERT_STR_EQ(connections[3].local_addr, "0.0.0.0:68");
  ASSERT_STR_EQ(connections[3].state, "UNCONN");

  return 0;
}

static int test_connection_summary_model(void) {
  TrfxConnectionSummaryResult result;
  ConnectionInfo connections[2];

  trfx_init_connection_summary_result(&result);
  ASSERT_INT_EQ(result.status, 0);
  ASSERT_INT_EQ(result.count, 0);

  ASSERT_INT_EQ(trfx_collect_connection_summary(NULL, 0, &result, NULL, 0), 0);
  ASSERT_INT_EQ(result.count, 0);

  memset(connections, 0, sizeof(connections));
  snprintf(connections[0].protocol, sizeof(connections[0].protocol), "TCP");
  snprintf(connections[0].local_addr, sizeof(connections[0].local_addr),
           "127.0.0.1:8080");
  snprintf(connections[0].remote_addr, sizeof(connections[0].remote_addr),
           "127.0.0.2:443");
  snprintf(connections[0].state, sizeof(connections[0].state), "ESTABLISHED");
  snprintf(connections[0].pid, sizeof(connections[0].pid), "42");
  snprintf(connections[0].process, sizeof(connections[0].process), "curl");
  snprintf(connections[0].user, sizeof(connections[0].user), "root");
  connections[0].uid = 0;

  snprintf(connections[1].protocol, sizeof(connections[1].protocol), "UDP");
  snprintf(connections[1].local_addr, sizeof(connections[1].local_addr),
           "[::1]:53");
  snprintf(connections[1].remote_addr, sizeof(connections[1].remote_addr),
           "[::]:0");
  snprintf(connections[1].state, sizeof(connections[1].state), "UNCONN");
  snprintf(connections[1].pid, sizeof(connections[1].pid), "-");
  snprintf(connections[1].process, sizeof(connections[1].process), "-");
  snprintf(connections[1].user, sizeof(connections[1].user), "-");
  connections[1].uid = 1000;

  ASSERT_INT_EQ(trfx_collect_connection_summary(connections, 2, &result, NULL,
                                                0),
                2);
  ASSERT_INT_EQ(result.count, 2);
  ASSERT_STR_EQ(result.rows[0].protocol, "TCP");
  ASSERT_STR_EQ(result.rows[0].state, "ESTABLISHED");
  ASSERT_STR_EQ(result.rows[0].local_endpoint, "127.0.0.1:8080");
  ASSERT_STR_EQ(result.rows[0].remote_endpoint, "127.0.0.2:443");
  ASSERT_STR_EQ(result.rows[0].pid, "42");
  ASSERT_STR_EQ(result.rows[0].process, "curl");
  ASSERT_INT_EQ(result.rows[0].has_owner, 1);
  ASSERT_INT_EQ(result.rows[0].is_established, 1);
  ASSERT_INT_EQ(result.rows[0].is_listener, 0);
  ASSERT_INT_EQ(result.rows[0].is_ipv6, 0);
  ASSERT_STR_EQ(result.rows[1].protocol, "UDP");
  ASSERT_STR_EQ(result.rows[1].state, "UNCONN");
  ASSERT_INT_EQ(result.rows[1].has_owner, 0);
  ASSERT_INT_EQ(result.rows[1].is_listener, 1);
  ASSERT_INT_EQ(result.rows[1].is_ipv6, 1);

  return 0;
}

static int test_connection_focus_state(void) {
  TrfxConnectionSummaryResult result;
  TrfxConnectionSummaryResult copied;
  int focus_index = -1;

  trfx_init_connection_summary_result(&result);
  snprintf(result.rows[0].protocol, sizeof(result.rows[0].protocol), "TCP");
  snprintf(result.rows[0].state, sizeof(result.rows[0].state), "ESTABLISHED");
  snprintf(result.rows[0].local_endpoint, sizeof(result.rows[0].local_endpoint),
           "127.0.0.1:80");
  snprintf(result.rows[0].remote_endpoint,
           sizeof(result.rows[0].remote_endpoint), "127.0.0.2:443");
  snprintf(result.rows[0].pid, sizeof(result.rows[0].pid), "11");
  snprintf(result.rows[0].process, sizeof(result.rows[0].process), "nginx");
  result.rows[0].has_owner = 1;
  result.rows[0].is_established = 1;

  snprintf(result.rows[1].protocol, sizeof(result.rows[1].protocol), "UDP");
  snprintf(result.rows[1].state, sizeof(result.rows[1].state), "UNCONN");
  snprintf(result.rows[1].local_endpoint, sizeof(result.rows[1].local_endpoint),
           "[::1]:53");
  snprintf(result.rows[1].remote_endpoint,
           sizeof(result.rows[1].remote_endpoint), "[::]:0");
  result.rows[1].is_listener = 1;
  result.count = 2;

  trfx_connection_state_init();
  trfx_connection_state_update(&result);

  ASSERT_INT_EQ(trfx_connection_state_copy(&copied, &focus_index), 1);
  ASSERT_INT_EQ(copied.count, 2);
  ASSERT_INT_EQ(focus_index, 0);

  trfx_connection_state_move_focus(1);
  ASSERT_INT_EQ(trfx_connection_state_copy(&copied, &focus_index), 1);
  ASSERT_INT_EQ(focus_index, 1);

  trfx_connection_state_move_focus(1);
  ASSERT_INT_EQ(trfx_connection_state_copy(&copied, &focus_index), 1);
  ASSERT_INT_EQ(focus_index, 0);

  trfx_connection_state_move_focus(-1);
  ASSERT_INT_EQ(trfx_connection_state_copy(&copied, &focus_index), 1);
  ASSERT_INT_EQ(focus_index, 1);

  return 0;
}

static int test_socket_owner_inode_lookup(void) {
  TrfxSocketOwnerMapEntry owners[] = {
      {12345, "42", "curl"},
      {23456, "77", "sshd"},
  };
  char pid[16];
  char process[64];

  ASSERT_INT_EQ(trfx_find_socket_owner_by_inode(owners, 2, 23456, pid,
                                                sizeof(pid), process,
                                                sizeof(process)),
                1);
  ASSERT_STR_EQ(pid, "77");
  ASSERT_STR_EQ(process, "sshd");

  ASSERT_INT_EQ(trfx_find_socket_owner_by_inode(owners, 2, 99999, pid,
                                                sizeof(pid), process,
                                                sizeof(process)),
                0);
  ASSERT_STR_EQ(pid, "-");
  ASSERT_STR_EQ(process, "-");

  return 0;
}

static int test_parse_socket_owner_fixture(void) {
  TrfxSocketOwnerMapEntry owner_map[] = {
      {12345, "42", "curl"},
      {23456, "77", "sshd"},
  };
  SocketOwnerInfo owners[MAX_SOCKET_OWNERS];

  int count = trfx_parse_socket_owner_path(
      "tests/fixtures/proc_net_tcp", "TCP", owner_map, 2, owners, 0,
      MAX_SOCKET_OWNERS);

  ASSERT_INT_EQ(count, 2);
  ASSERT_STR_EQ(owners[0].proto, "TCP");
  ASSERT_STR_EQ(owners[0].pid, "42");
  ASSERT_STR_EQ(owners[0].process, "curl");
  ASSERT_STR_EQ(owners[0].laddr, "127.0.0.1");
  ASSERT_STR_EQ(owners[0].lport, "8080");
  ASSERT_STR_EQ(owners[0].raddr, "127.0.0.2");
  ASSERT_STR_EQ(owners[0].rport, "443");
  ASSERT_STR_EQ(owners[1].pid, "77");
  ASSERT_STR_EQ(owners[1].process, "sshd");
  ASSERT_STR_EQ(owners[1].laddr, "0.0.0.0");
  ASSERT_STR_EQ(owners[1].lport, "22");

  return 0;
}

static int test_tcp_state_names(void) {
  ASSERT_STR_EQ(trfx_tcp_state_name(1), "ESTABLISHED");
  ASSERT_STR_EQ(trfx_tcp_state_name(2), "SYN_SENT");
  ASSERT_STR_EQ(trfx_tcp_state_name(3), "SYN_RECV");
  ASSERT_STR_EQ(trfx_tcp_state_name(4), "FIN_WAIT1");
  ASSERT_STR_EQ(trfx_tcp_state_name(5), "FIN_WAIT2");
  ASSERT_STR_EQ(trfx_tcp_state_name(6), "TIME_WAIT");
  ASSERT_STR_EQ(trfx_tcp_state_name(7), "CLOSE");
  ASSERT_STR_EQ(trfx_tcp_state_name(8), "CLOSE_WAIT");
  ASSERT_STR_EQ(trfx_tcp_state_name(9), "LAST_ACK");
  ASSERT_STR_EQ(trfx_tcp_state_name(10), "LISTEN");
  ASSERT_STR_EQ(trfx_tcp_state_name(11), "CLOSING");
  ASSERT_STR_EQ(trfx_tcp_state_name(12), "NEW_SYN_RECV");
  ASSERT_STR_EQ(trfx_tcp_state_name(99), "UNKNOWN");

  return 0;
}

static int test_parse_ipv6_connection_fixtures(void) {
  ConnectionInfo connections[MAX_CONNECTIONS];

  int count = trfx_parse_connection_path("tests/fixtures/proc_net_tcp6", "TCP",
                                         connections, 0, MAX_CONNECTIONS);
  ASSERT_INT_EQ(count, 2);
  ASSERT_STR_EQ(connections[0].protocol, "TCP");
  ASSERT_STR_EQ(connections[0].local_addr, "[::1]:8080");
  ASSERT_STR_EQ(connections[0].remote_addr, "[::2]:443");
  ASSERT_STR_EQ(connections[0].state, "ESTABLISHED");
  ASSERT_INT_EQ((int)connections[0].inode, 56789);
  ASSERT_INT_EQ((int)connections[0].uid, 1000);
  ASSERT_STR_EQ(connections[1].local_addr, "[::]:22");
  ASSERT_STR_EQ(connections[1].remote_addr, "[::]:0");
  ASSERT_STR_EQ(connections[1].state, "LISTEN");

  count = trfx_parse_connection_path("tests/fixtures/proc_net_udp6", "UDP",
                                     connections, count, MAX_CONNECTIONS);
  ASSERT_INT_EQ(count, 3);
  ASSERT_STR_EQ(connections[2].protocol, "UDP");
  ASSERT_STR_EQ(connections[2].local_addr, "[::1]:53");
  ASSERT_STR_EQ(connections[2].remote_addr, "[::]:0");
  ASSERT_STR_EQ(connections[2].state, "UNCONN");

  return 0;
}

static int test_udp_state_names(void) {
  ASSERT_STR_EQ(trfx_udp_state_name(1), "ESTABLISHED");
  ASSERT_STR_EQ(trfx_udp_state_name(7), "UNCONN");
  ASSERT_STR_EQ(trfx_udp_state_name(99), "UNKNOWN");

  return 0;
}

int main(void) {
  if (test_parse_connection_fixtures() != 0)
    return 1;

  if (test_connection_summary_model() != 0)
    return 1;

  if (test_connection_focus_state() != 0)
    return 1;

  if (test_tcp_state_names() != 0)
    return 1;

  if (test_udp_state_names() != 0)
    return 1;

  if (test_parse_ipv6_connection_fixtures() != 0)
    return 1;

  if (test_socket_owner_inode_lookup() != 0)
    return 1;

  if (test_parse_socket_owner_fixture() != 0)
    return 1;

  return 0;
}
