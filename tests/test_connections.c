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

static int test_parse_connection_fixtures(void) {
  ConnectionInfo connections[MAX_CONNECTIONS];

  int count = trfx_parse_connection_path("tests/fixtures/proc_net_tcp", "TCP",
                                         connections, 0, MAX_CONNECTIONS);
  ASSERT_INT_EQ(count, 2);
  ASSERT_STR_EQ(connections[0].protocol, "TCP");
  ASSERT_STR_EQ(connections[0].local_addr, "127.0.0.1:8080");
  ASSERT_STR_EQ(connections[0].remote_addr, "127.0.0.2:443");
  ASSERT_STR_EQ(connections[0].state, "ESTABLISHED");
  ASSERT_STR_EQ(connections[1].protocol, "TCP");
  ASSERT_STR_EQ(connections[1].local_addr, "0.0.0.0:22");
  ASSERT_STR_EQ(connections[1].remote_addr, "0.0.0.0:0");
  ASSERT_STR_EQ(connections[1].state, "LISTEN");

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

static int test_udp_state_names(void) {
  ASSERT_STR_EQ(trfx_udp_state_name(1), "ESTABLISHED");
  ASSERT_STR_EQ(trfx_udp_state_name(7), "UNCONN");
  ASSERT_STR_EQ(trfx_udp_state_name(99), "UNKNOWN");

  return 0;
}

int main(void) {
  if (test_parse_connection_fixtures() != 0)
    return 1;

  if (test_tcp_state_names() != 0)
    return 1;

  if (test_udp_state_names() != 0)
    return 1;

  return 0;
}
