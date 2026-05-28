/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_socket_owners.h"

#include <stdio.h>
#include <time.h>

static long elapsed_us(struct timespec start, struct timespec end) {
  long seconds = end.tv_sec - start.tv_sec;
  long nanoseconds = end.tv_nsec - start.tv_nsec;

  return seconds * 1000000L + nanoseconds / 1000L;
}

int main(void) {
  TrfxSocketOwnerMapEntry entries[MAX_SOCKET_OWNER_MAP_ENTRIES];
  struct timespec start;
  struct timespec end;

  if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
    perror("clock_gettime");
    return 1;
  }

  int count = trfx_scan_socket_owner_map(entries, MAX_SOCKET_OWNER_MAP_ENTRIES);

  if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
    perror("clock_gettime");
    return 1;
  }

  printf("socket_owner_scan entries=%d elapsed_us=%ld\n", count,
         elapsed_us(start, end));
  return 0;
}
