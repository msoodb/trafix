<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- Copyright (C) 2025 Masoud Bolhassani. -->

# Trafix - Technical Documentation

## Overview

**Trafix** is a lightweight Linux terminal monitoring tool. It currently
provides an ncurses TUI for system, CPU, memory, disk, process, connection,
network, and socket-owner visibility.

The command line supports both the interactive TUI entry point and scriptable
read-only commands:

- `trafix` launches the interactive TUI.
- `trafix --help` or `trafix -h` prints usage.
- `trafix --version` or `trafix -v` prints version information.
- `trafix interfaces` prints network interface counters.
- `trafix connections` prints current TCP/UDP connection rows.
- `trafix listeners` prints local TCP listeners and UDP unconnected sockets.
- `trafix system` prints a compact system overview.
- `--json` selects JSON output for scriptable commands.
- `trafix connections --proto tcp|udp` filters by protocol.
- `trafix connections --state STATE` filters by connection state.
- Unknown arguments fail clearly and do not launch the TUI.

Inside the TUI, the hotkey help is shown in a popup opened with `F1`, `h`, or
`H`. The popup is dismissed with `Esc`, `Enter`, or `q`.

The shipped filter surface is limited to `connections --proto` and
`connections --state`. Alerts, top-talkers, and per-process bandwidth are
roadmap items, not current behavior.

## Build Dependencies

- Linux
- C compiler such as `gcc`
- `make`
- ncurses development package

Optional runtime tools may improve some panels:

- `lm_sensors` for CPU temperature data
- `iw` for Wi-Fi details
- `iproute2` for route information
- `procps` for process listings

Trafix does not currently use libpcap, eBPF, or privileged packet capture.

## Build and Install

Build from source:

```sh
make
```

Run from the source tree:

```sh
bin/trafix
```

Install:

```sh
sudo make install
```

Run after installation:

```sh
trafix
```

Check version:

```sh
trafix --version
```

## Current Runtime Architecture

The executable parses CLI arguments first. For no-argument TUI mode, it reads
configuration and starts the dashboard.

The TUI is organized around ncurses windows. Several worker threads collect data
and render panels:

- system overview
- CPU information
- memory usage
- disk usage
- active connections
- network information
- process information
- socket owners

Collectors currently read from Linux sources such as `/proc`, `/sys`,
`getifaddrs(3)`, `statvfs(3)`, `uname(2)`, and `utmpx`.

## Current TUI Panels

### System

Shows hostname, OS, kernel, uptime, load averages, and logged-in users.

### CPU

Shows average CPU usage, per-core usage, CPU frequency, and temperature when
available.

### Memory

Shows RAM and swap usage.

### Disk

Shows mounted filesystem usage and totals.

### Connections

Shows current TCP/UDP connection rows parsed from `/proc/net/tcp`,
`/proc/net/udp`, `/proc/net/tcp6`, and `/proc/net/udp6`.

### Network

Shows default route information, DNS servers, active interface details, optional
Wi-Fi details, VPN interface detection, and interface-level byte rates.

### Processes

Shows process data gathered from `ps`.

### Socket Owners

Shows sockets mapped to owning PID/process where visible from `/proc/*/fd`.
This panel does **not** measure per-socket bandwidth.

## Bandwidth Scope

Trafix currently reports bandwidth only as interface-level byte rates calculated
from network interface counters. Connection, listener, and socket-owner views
show ownership metadata where permissions allow; they do not report
per-connection, per-socket, or per-process byte counts.

## Configuration

The default configuration path is:

```sh
/etc/trafix/config.cfg
```

Current options:

- `TEMP_WARN_YELLOW`: CPU temperature warning threshold.
- `TEMP_WARN_RED`: CPU temperature critical threshold.
- `ROW2_MODULES`: number of second-row TUI modules, from 1 to 3.
- `SHOW_TOP_PANELS`: enable or disable the top system, CPU, memory, and disk
  panel row together.
- `TUI_REFRESH_INTERVAL_MS`: standard TUI panel refresh cadence.
- `TUI_PAUSE_INTERVAL_MS`: sleep cadence while the TUI is paused.
- `TUI_READY_CHECK_INTERVAL_MS`: worker startup readiness check cadence.
- `TUI_SMALL_PANEL_REFRESH_MS`: refresh cadence for small-window fallback messages.

Example:

```ini
TEMP_WARN_YELLOW = 50
TEMP_WARN_RED = 75
ROW2_MODULES = 3
SHOW_TOP_PANELS = 1
TUI_REFRESH_INTERVAL_MS = 1000
TUI_PAUSE_INTERVAL_MS = 100
TUI_READY_CHECK_INTERVAL_MS = 10
TUI_SMALL_PANEL_REFRESH_MS = 2000
```

## Current CLI

```sh
trafix
trafix --help
trafix --version
trafix interfaces
trafix interfaces --json
trafix connections
trafix connections --json
trafix connections --proto tcp
trafix connections --state ESTABLISHED
trafix listeners
trafix listeners --json
trafix system
trafix system --json
```

The scriptable commands use text output by default and JSON output with
`--json`.

## Roadmap

Planned future work includes:

- cleaner TUI lifecycle and resize handling
- packaging and CI improvements

Alerts, top-talkers, per-process bandwidth accounting, libpcap, eBPF, remote
agents, and historical metrics are not implemented.

## License

Trafix is licensed under the GPL-3.0-or-later License. See the [LICENSE](../LICENSE)
file for more details.
