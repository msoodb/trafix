<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- Copyright (C) 2025 Masoud Bolhassani -->

![Trafix Dashboard](./trafix.png)

# Trafix

**Trafix** - A Lightweight Linux Monitoring Tool

Trafix is a lightweight terminal monitoring tool for Linux systems. It provides an interactive TUI for system, process, connection, and network visibility, plus scriptable CLI commands for interface counters, connections, and system overview data.

## Table of Contents

- [Key Features](#key-features)
- [Installation and Usage](#installation-and-usage)
  - [Install from Source](#install-from-source)
  - [Install from Fedora Repository](#install-from-fedora-repository)
  - [Debian and Ubuntu Packaging](#debian-and-ubuntu-packaging)
- [User Manual](#user-manual)
  - [Hotkeys](#hotkeys)
  - [Configuration](#configuration)
    - [Example Configuration File](#example-configuration-file)
    - [Configuration Options](#configuration-options)
- [Roadmap](#roadmap)

## Key Features:

- **Monitor Active Connections:** View detailed information about all active TCP/UDP connections, including local and remote addresses, ports, and connection states.
- **Track Interface Activity:** Monitor interface-level sent/received byte rates from Linux interface counters.
- **Socket Owners:** Map visible sockets to owning PID/process where available.
- **Scriptable CLI:** Print interface counters, active connections, and system overview data in text or JSON format.

Trafix is designed to be lightweight, efficient, and to use minimal system resources, making it an ideal tool for monitoring network activity on Linux-based systems.

## Bandwidth Scope

In this release line, Trafix treats bandwidth as interface-level byte rates. Connection and socket-owner views show protocol, address, state, UID/user, PID, and process ownership where visible, but they do not claim per-connection, per-socket, or per-process byte accounting.

## Build Requirements

- Linux
- C compiler such as `gcc`
- `make`
- ncurses development package

Optional runtime tools that improve some panels:

- `lm_sensors` for CPU temperature data
- `iw` for Wi-Fi details
- `iproute2` for route information
- `procps` for process listings

## Installation and Usage:

### Install from Source

To install Trafix from the source, follow these steps:

1. Clone the repository:

    ```sh
    git clone https://github.com/msoodb/trafix.git
    cd trafix
    ```

2. Build and install:

    ```sh
    make clean
    make
    sudo make install
    ```

3. After installation, you can run Trafix with:

    ```sh
    trafix
    ```

    When running from the source tree without installing, use:

    ```sh
    bin/trafix
    ```

### Command-line Usage

Launch the Trafix TUI:

```sh
trafix
```

Show help:

```sh
trafix --help
```

Show version information:

```sh
trafix --version
```

Print network interface counters:

```sh
trafix interfaces
trafix interfaces --json
```

Print active TCP/UDP connections:

```sh
trafix connections
trafix connections --json
trafix connections --proto tcp
trafix connections --state ESTABLISHED
```

Print local listeners:

```sh
trafix listeners
trafix listeners --json
```

Print a compact system overview:

```sh
trafix system
trafix system --json
```

CLI commands use text output by default. Use `--json` when integrating Trafix with scripts or other tools.

### Install from Fedora Repository
> This installation method is under development and not yet ready for use.

1. If you're using Fedora or a compatible distribution, you can install Trafix directly from the Fedora repository:

	```sh
	sudo dnf install trafix
	```

2. After installation, run Trafix with:
	```sh
	trafix
	```

### Debian and Ubuntu Packaging

Debian and Ubuntu packages are not available yet. See
`docs/debian_packaging.md` for the current packaging roadmap and dependency
notes.

## User Manual

Trafix offers an interactive command-line interface with real-time controls. You can manage views, sorting, and behavior using the following keyboard shortcuts:

For release and regression checks, use the manual TUI checklist in
`docs/tui_manual_test_checklist.md`.

### Hotkeys

- `[F1]`, `[h]`, `[H]` — **Help Popup:** Show the hotkey help popup.
- `[Esc]`, `[Enter]`, `[q]` — **Close Popup:** Dismiss the hotkey popup.
- `[1]`, `[2]`, `[3]` — **Switch Panels:** Toggle between different dashboard views.
- `[s]` — **Sort Processes:** Change the sorting order of process information.
- `[r]` — **Refresh:** Force a manual refresh of all panels.
- `[c]` — **Columns:** Toggle or cycle through different column views in specific panels.
- `[p]` — **Pause:** Pause/resume real-time updates.
- `[q]` — **Quit:** Exit the Trafix application.

### Configuration

All default settings can be customized by editing the configuration file:

```sh
sudo nano /etc/trafix/config.cfg
```

### Example Configuration File
```
# config/config.cfg

# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2025 Masoud Bolhassani.


TEMP_WARN_YELLOW = 50
TEMP_WARN_RED = 75

ROW2_MODULES = 3
SHOW_TOP_PANELS = 1
TUI_REFRESH_INTERVAL_MS = 1000
TUI_PAUSE_INTERVAL_MS = 100
TUI_READY_CHECK_INTERVAL_MS = 10
TUI_SMALL_PANEL_REFRESH_MS = 2000
```

### Configuration Options

- **TEMP_WARN_YELLOW** *(default: 50)*  
  Temperature threshold in °C that triggers a yellow warning in the UI, indicating a moderate temperature.

- **TEMP_WARN_RED** *(default: 75)*  
  Temperature threshold in °C that triggers a red warning in the UI, indicating a high or dangerous temperature.

- **ROW2_MODULES** *(default: 3)*  
  Number of columns (modules) shown in the second row of the dashboard. Adjust this to control layout density (1 to 3).

- **SHOW_TOP_PANELS** *(default: 1)*  
  Controls whether the top system, CPU, memory, and disk panels are shown.

- **TUI_REFRESH_INTERVAL_MS** *(default: 1000)*  
  Standard refresh cadence for TUI panels, in milliseconds.

- **TUI_PAUSE_INTERVAL_MS** *(default: 100)*  
  Sleep cadence while the TUI is paused, in milliseconds.

- **TUI_READY_CHECK_INTERVAL_MS** *(default: 10)*  
  Startup wait cadence while worker threads wait for the TUI runtime to become ready, in milliseconds.

- **TUI_SMALL_PANEL_REFRESH_MS** *(default: 2000)*  
  Refresh cadence for small-window fallback messages, in milliseconds.

After modifying the configuration, simply exit and run Trafix again to apply the changes:

## Roadmap

The long-term plan lives in [`docs/roadmap.md`](docs/roadmap.md). The
phase-by-phase task lists live under [`taklists/`](taklists/).
