/*
 * Copyright (C) 2025 Masoud Bolhassani <masoud.bolhassani@gmail.com>
 *
 * This file is part of Trafix.
 *
 * Trafix is released under the GNU General Public License v3 (GPL-3.0).
 * See LICENSE file for details.
 */

#include "trfx_version.h"

#ifndef TRFX_VERSION
#define TRFX_VERSION "unknown"
#endif

const char *trfx_get_version(void) {
  return TRFX_VERSION;
}
