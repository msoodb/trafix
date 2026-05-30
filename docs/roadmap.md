<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- Copyright (C) 2025 Masoud Bolhassani. -->

# Trafix Roadmap

Trafix is already a working Linux observability TUI and CLI. The next stages
should turn it into a serious operator tool for understanding, drilling into,
and acting on network problems without turning the codebase into a pile of
special cases.

The roadmap is organized around four goals:

1. Observe what the machine and network are doing.
2. Explain why a connection, socket, or host looks suspicious.
3. Act on a problem safely and deliberately.
4. Keep the UI fast, readable, and stable in narrow terminals.

The phase task lists live under `taklists/` and are designed to stay small
enough to implement, verify, and commit one task at a time.

## Phase 8: Network Observability Foundation

Build a clearer network-first view of the machine:

- normalize interface, route, socket, and listener data
- make the top-level network picture easier to scan
- add stable drill-down views for connections and sockets
- keep narrow-layout rendering clean

## Phase 9: Socket And Bandwidth Depth

Add the deeper network answers an operator usually asks next:

- identify top talkers and active flows
- estimate bandwidth at the socket or process level where the kernel allows it
- show a focused suspect-connection view
- preserve a clear fallback when the system cannot expose a measurement

## Phase 10: Safe Network Actions

Add controlled intervention paths:

- kill a process from the TUI or CLI when the operator explicitly chooses it
- close or drop a suspicious socket or connection when the platform allows it
- require confirmation and permission checks for destructive actions
- record what action was taken and why

## Phase 11: Diagnostics And Correlation

Make Trafix better at explaining the cause of a network problem:

- pull in logs and recent diagnostics context
- correlate network symptoms with CPU, memory, and disk pressure
- surface route, DNS, and interface health clues
- produce a compact troubleshooting snapshot

## Phase 12: Polish, Automation, And Release Hardening

Make the tool feel complete and maintainable:

- add alerts and thresholds for important states
- add saved profiles and repeatable operator workflows
- add performance and regression checks
- keep packaging, docs, and examples aligned with shipped behavior

## Operating Rule

Do not promote a feature from roadmap to shipped behavior until it has:

- an implementation
- a local verification path
- documentation that matches the implementation
- a task list item that has been completed and reviewed

