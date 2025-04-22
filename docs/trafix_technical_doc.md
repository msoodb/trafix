
# Trafix - Technical Documentation

## Overview

**Trafix** is a lightweight, real-time network monitoring tool designed for Linux systems. It provides visibility into network activity, allowing you to track bandwidth usage, monitor active connections, and identify top traffic sources. Trafix operates via the command-line interface (CLI), offering a simple yet powerful way to monitor your system's network performance.

## Table of Contents

1. [Installation](#installation)
2. [Usage](#usage)
   - [Basic Commands](#basic-commands)
   - [Advanced Options](#advanced-options)
3. [Features](#features)
4. [Configuration](#configuration)
5. [Examples](#examples)
6. [Contributing](#contributing)
7. [License](#license)

## Installation

### Prerequisites

Before installing **Trafix**, ensure you have the following dependencies:

- **Linux system** (Ubuntu, Debian, CentOS, etc.)
- **gcc** (GNU Compiler Collection)
- **make** (for building the tool from source)
- **libpcap-dev** (for packet capture, if applicable)

### Installing from Source

To install **Trafix** from source, follow these steps:

1. Clone the repository:
   ```bash
   git clone https://github.com/msoodb/trafix.git
   cd trafix
   ```

2. Compile the code:
   ```bash
   make
   ```

3. Install the tool:
   ```bash
   sudo make install
   ```

4. Verify the installation:
   ```bash
   trafix --version
   ```

This will display the installed version of **Trafix**.

### Installing via Package Manager

If you're using a Linux distribution that supports package management, you can also install **Trafix** through the package manager (e.g., `apt`, `yum`, etc.), assuming it's available on your distribution's repository.

```bash
sudo apt install trafix  # For Debian-based systems
sudo yum install trafix  # For Red Hat-based systems
```

## Usage

### Basic Commands

Once installed, you can use **Trafix** from the command line. Below are some of the basic commands:

- **Start Monitoring:**
  ```bash
  trafix start
  ```

  This command begins monitoring network activity in real time.

- **Monitor Active Connections:**
  ```bash
  trafix connections
  ```

  Lists all active network connections, including local and remote addresses, ports, and connection statuses.

- **Monitor Bandwidth Usage:**
  ```bash
  trafix bandwidth
  ```

  Displays real-time bandwidth usage, including total incoming and outgoing traffic.

### Advanced Options

- **Set Bandwidth Alert:**
  ```bash
  trafix --alert-bandwidth 100MB
  ```

  Sets a bandwidth usage threshold of 100MB. An alert will trigger when this threshold is exceeded.

- **Filter By Process:**
  ```bash
  trafix --filter-process nginx
  ```

  Filters the output to show only network connections related to the specified process (in this case, `nginx`).

- **View Top Talkers:**
  ```bash
  trafix top
  ```

  Displays the top processes or IP addresses consuming the most bandwidth.

## Features

- **Real-Time Monitoring**: Provides live data on active network connections and bandwidth usage.
- **Detailed Connection Information**: Displays local and remote addresses, ports, protocols (TCP/UDP), and process IDs (PIDs) associated with connections.
- **Bandwidth Tracking**: Real-time tracking of incoming and outgoing network traffic with detailed statistics.
- **Alerts**: Allows users to set custom thresholds for network activity and get notified when thresholds are exceeded.
- **Top Talkers**: Identify which processes or IP addresses are using the most bandwidth in real-time.
- **Minimal Dependencies**: Built to be lightweight and fast, with minimal system resource usage.

## Configuration

**Trafix** can be configured via command-line options or through a configuration file. The configuration file is optional but can be used for persistent settings.

### Configuration File

The configuration file can be located at `/etc/trafix/config`. Here is an example configuration:

```ini
[settings]
alert_bandwidth = 100MB
alert_process = nginx
```

### Command-Line Arguments

- `--alert-bandwidth`: Set a threshold for bandwidth usage. Alerts are triggered if this limit is exceeded.
- `--filter-process`: Filter results by a specific process.
- `--top`: Display the top talkers consuming bandwidth.
- `--connections`: List all active network connections.

## Examples

### Example 1: Monitor Active Connections

To monitor all active network connections, run:

```bash
trafix connections
```

This will display a list of active connections along with their associated details such as IP addresses, ports, and connection status.

### Example 2: Monitor Bandwidth Usage

To monitor bandwidth usage in real-time:

```bash
trafix bandwidth
```

This command will display incoming and outgoing bandwidth usage on all network interfaces.

### Example 3: Set Bandwidth Alert

To set a bandwidth alert for 100MB:

```bash
trafix --alert-bandwidth 100MB
```

An alert will be triggered if the total bandwidth usage exceeds 100MB.

### Example 4: View Top Talkers

To view which processes or IP addresses are consuming the most bandwidth:

```bash
trafix top
```

This will show the processes or IP addresses that are currently using the most network resources.

## Contributing

We welcome contributions to **Trafix**! If you'd like to contribute, please fork the repository and submit a pull request with your changes. Be sure to write tests for any new features or bug fixes.

### Steps to Contribute:
1. Fork the repository.
2. Clone your fork to your local machine.
3. Make your changes and commit them.
4. Push your changes to your fork.
5. Open a pull request with a description of your changes.

## License

**Trafix** is licensed under the MIT License. See the [LICENSE](LICENSE) file for more details.
