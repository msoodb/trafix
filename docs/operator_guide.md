<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- Copyright (C) 2025 Masoud Bolhassani. -->

# Trafix Operator Guide

This guide is for the workflow Trafix already supports today: observe the
network, drill into a suspect flow, take a controlled action, and check system
pressure when the network does not look healthy.

## Observe

Start the TUI with:

```sh
trafix
```

Use the Network panel to get the first read on the host:

- default route
- DNS servers
- active interface
- VPN clue
- top talkers
- recent sample trend

Use the Connections panel when you need the raw socket table and the grouped
summary by protocol, state, and ownership.

## Drill Down

In the Network panel:

- `j` and `k` move the selected top talker
- `d` or `Enter` opens the bandwidth detail popup for the selected entry

Use the Socket Owners panel when you need PID, process, and endpoint context
for owned sockets.

## Act

Trafix supports controlled actions, not blind kills:

- `x` opens the process kill flow
- `z` opens the connection or socket drop flow
- `a` opens the recent action audit popup

Both the TUI and the CLI require confirmation for destructive actions unless
`--yes` is supplied on the command line.

## Diagnose

Use the diagnostics views when the symptom is not obvious:

- `g` opens the troubleshooting snapshot popup
- `n` opens route and DNS checks
- `v` opens network health correlation
- `trafix diagnostics` prints a text troubleshooting snapshot

The diagnostics output combines system, network, log, CPU, memory, disk, and
process pressure context. It also surfaces concise alerts for supported
thresholds configured in `/etc/trafix/config.cfg` or a saved profile.

## Keep It Practical

Use the source of truth already in the tree:

- `man trafix` for command reference
- `docs/tui_manual_test_checklist.md` for release checks
- `docs/trafix_technical_doc.md` for implementation detail

Do not expect unshipped features such as remote agents, libpcap capture, or
historical long-term retention.
