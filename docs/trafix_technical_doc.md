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
- `trafix diagnostics` prints a troubleshooting snapshot with system, network,
  log, and pressure context.
- `--json` selects JSON output for scriptable commands.
- `trafix connections --proto tcp|udp` filters by protocol.
- `trafix connections --state STATE` filters by connection state.
- Unknown arguments fail clearly and do not launch the TUI.

Inside the TUI, the hotkey help is shown in a popup opened with `F1`, `h`, or
`H`. The popup lists the keys with short descriptions and is dismissed with
`Esc`, `Enter`, or `q`.
The `t` key toggles the top system, CPU, memory, and disk panels at runtime;
the initial state still comes from `SHOW_TOP_PANELS` in the config file.
The `g` key shows the troubleshooting diagnostics in the support dock.
The `n` key shows the route and DNS checks in the support dock.
The `v` key shows the network health correlation in the support dock.
The `l` key cycles the support dock view.
In the Network panel, `j` and `k` move the selected top talker, and `d` or
`Enter` shows the bandwidth detail in the support dock.
The `x` key opens a process chooser and requests a kill after confirmation.
The `z` key opens a connection chooser and requests a drop where supported.
The `a` key shows the recent action audit in the support dock.
The shipped filter surface is limited to `connections --proto` and
`connections --state`. Alerts and remote agents remain roadmap items, but
top-talkers, trend history, and estimated socket/process bandwidth are now
part of the shipped UI.
The dashboard is arranged as a primary column for the core overview modules
and a support column for logs, diagnostics, audits, and other live support
data. On narrow terminals, the support column collapses automatically instead
of forcing cramped rendering.
The diagnostics snapshot intentionally pulls together system overview, route,
DNS, recent log lines, CPU, memory, disk, and process pressure so the
operator can explain a problem without jumping across panels.

Action handling is intentionally conservative:

- the TUI asks for confirmation before kill or drop actions run
- the CLI requires explicit `--yes` for non-interactive action requests
- permission failures are reported clearly before or during execution
- unsupported connection or socket drops fail cleanly instead of pretending
  to succeed
- recent action outcomes are recorded in a small audit trail for debugging

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
- network overview and interface rates
- process information
- socket inventory drill-down

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
`/proc/net/udp`, `/proc/net/tcp6`, and `/proc/net/udp6`, plus a compact summary
grouped by protocol, state, and ownership.

### Network

Shows a compact top-level network overview with default route information, DNS
servers, active interface details, optional Wi-Fi details, VPN interface
detection, interface-level byte rates, a top-talker summary, and a short
sample-based trend history. The selected top talker can be opened in a detail
popup.

### Processes

Shows process data gathered from `ps`.

### Socket Owners

Shows a socket inventory drill-down focused on owned sockets, PID/process
context, and local/remote endpoints where visible from `/proc/*/fd`.
This panel does **not** measure per-socket bandwidth.

## Bandwidth Scope

Trafix estimates bandwidth from interface counters and ownership metadata.
When connections or socket owners are visible, the UI can present top talkers
and a detail popup for the selected flow. When ownership data is missing, the
UI keeps the fallback explicit rather than claiming exact per-socket counts.

## Configuration

The default configuration path is:

```sh
/etc/trafix/config.cfg
```

Current options:

- `TEMP_WARN_YELLOW`: CPU temperature warning threshold.
- `TEMP_WARN_RED`: CPU temperature critical threshold.
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
