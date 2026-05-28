/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdio.h>
#include <string.h>

#define ASSERT_STR_EQ(actual, expected)                                        \
  do {                                                                        \
    if (strcmp((actual), (expected)) != 0) {                                   \
      fprintf(stderr, "%s:%d: expected \"%s\", got \"%s\"\n", __FILE__,       \
              __LINE__, (expected), (actual));                                \
      return 1;                                                               \
    }                                                                         \
  } while (0)

#define ASSERT_INT_EQ(actual, expected)                                        \
  do {                                                                        \
    if ((actual) != (expected)) {                                             \
      fprintf(stderr, "%s:%d: expected %d, got %d\n", __FILE__, __LINE__,     \
              (expected), (actual));                                          \
      return 1;                                                               \
    }                                                                         \
  } while (0)

#define ASSERT_MODE_EQ(actual, expected)                                       \
  do {                                                                        \
    if ((actual) != (expected)) {                                             \
      fprintf(stderr, "%s:%d: expected CLI mode %d, got %d\n", __FILE__,     \
              __LINE__, (expected), (actual));                                \
      return 1;                                                               \
    }                                                                         \
  } while (0)

#endif // TEST_COMMON_H
