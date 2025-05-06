<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- Copyright (C) 2025 Masoud Bolhassani -->

![Trafix Dashboard](./trafix.png)

# Trafix

**Trafix** - A Lightweight Linux Monitoring Tool

Trafix is a high-performance, user-friendly monitoring tool for Linux systems. It enables real-time tracking of active network connections and provides insights into top traffic sources, all via a streamlined command-line interface.

## Table of Contents

- [Key Features](#key-features)
- [Installation and Usage](#installation-and-usage)
  - [Install from Source](#install-from-source)
  - [Install from Fedora Repository](#install-from-fedora-repository)
- [User Manual](#user-manual)
  - [Hotkeys](#hotkeys)
  - [Configuration](#configuration)
    - [Example Configuration File](#example-configuration-file)
    - [Configuration Options](#configuration-options)

## Key Features:

- **Monitor Active Connections:** View detailed information about all active TCP/UDP connections, including local and remote addresses, ports, and connection states.
- **Track Bandwidth Usage:** Monitor both incoming and outgoing network traffic in real-time.
- **Alerts & Thresholds:** Set custom thresholds for network usage and receive notifications when these limits are exceeded.
- **User-Friendly CLI:** A simple and intuitive command-line interface with filtering, sorting, and customizable options for tailored monitoring.

Trafix is designed to be lightweight, efficient, and to use minimal system resources, making it an ideal tool for monitoring network activity on Linux-based systems.

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
    bin/trafix
    ```

### Install from Fedora Repository

1. If you're using Fedora or a compatible distribution, you can install Trafix directly from the Fedora repository:

	```sh
	sudo dnf install trafix
	```

2. After installation, run Trafix with:
	```sh
	trafix
	```

## User Manual

Trafix offers an interactive command-line interface with real-time controls. You can manage views, sorting, and behavior using the following keyboard shortcuts:

### Hotkeys

- `[1]`, `[2]`, `[3]` — **Switch Panels:** Toggle between different dashboard views.
- `[s]` — **Sort Processes:** Change the sorting order of process information (e.g., by bandwidth).
- `[r]` — **Refresh:** Force a manual refresh of all panels.
- `[c]` — **Columns:** Toggle or cycle through different column views in specific panels.
- `[p]` — **Pause:** Pause/resume real-time updates.
- `[q]` — **Quit:** Exit the Trafix application.

### Configuration

All default settings can be customized by editing the configuration file:

```sh
sudo nano /etc/trafix/config.cfg
```

Example Configuration File
```
# config/config.cfg

# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2025 Masoud Bolhassani.


TEMP_WARN_YELLOW = 50
TEMP_WARN_RED = 75

ROW2_MODULES = 3
```

### Configuration Options

- **TEMP_WARN_YELLOW** *(default: 50)*  
  Temperature threshold in °C that triggers a yellow warning in the UI, indicating a moderate temperature.

- **TEMP_WARN_RED** *(default: 75)*  
  Temperature threshold in °C that triggers a red warning in the UI, indicating a high or dangerous temperature.

- **ROW2_MODULES** *(default: 3)*  
  Number of columns (modules) shown in the second row of the dashboard. Adjust this to control layout density (e.g., 1 to 4).

> After modifying the configuration, simply exit and run Trafix again to apply the changes:
