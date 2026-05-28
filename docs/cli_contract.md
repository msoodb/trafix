<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- Copyright (C) 2025 Masoud Bolhassani. -->

# Trafix CLI Contract

This document defines the scriptable Trafix CLI behavior implemented during
Phase 3.

## General Rules

- Running `trafix` with no arguments launches the TUI.
- Scriptable commands must not initialize ncurses.
- Output must be stable enough for simple shell parsing.
- Text output is the default.
- JSON output is available for scriptable commands with `--json`.
- Unknown commands or invalid options exit non-zero.

## Exit Codes

- `0`: command completed successfully.
- `1`: invalid arguments or general command failure.
- `2`: required collector data is unavailable or permission-limited.

## Initial Subcommands

### `trafix interfaces`

Purpose:
- Print current network interface counters.

Default text columns:

```text
INTERFACE  RX_BYTES  TX_BYTES
```

Rules:
- Include loopback unless a later filter option is added.
- Values are raw byte counters from `/proc/net/dev`.
- Do not label counters as rates unless rate calculation is explicitly added.

### `trafix connections`

Purpose:
- Print current TCP/UDP connection rows.

Default text columns:

```text
PROTO  LOCAL  REMOTE  STATE
```

Rules:
- Use the same tested connection parser as the TUI.
- Do not show fake bandwidth columns.
- IPv4 is expected first; IPv6 can be added later when the parser supports it.

### `trafix system`

Purpose:
- Print a compact system overview.

Default text fields:

```text
HOSTNAME
OS
KERNEL
UPTIME
LOAD_AVG
LOGGED_IN_USERS
```

Rules:
- Use the same system overview collector as the TUI.
- Missing fields should display `N/A`.

## JSON Output

Supported form:

```sh
trafix interfaces --json
trafix connections --json
trafix system --json
```

JSON output is valid JSON and uses stable field names.

## Connection Filters

Supported filters:

```sh
trafix connections --proto tcp
trafix connections --proto udp
trafix connections --state ESTABLISHED
trafix connections --state LISTEN
```

Filters must not affect the TUI unless explicitly added later.
