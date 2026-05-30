<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- Copyright (C) 2025 Masoud Bolhassani. -->

# Trafix Roadmap

Trafix is already a working Linux observability TUI and CLI. Phases 8 through
12 are shipped. The remaining work is now focused on deeper network overview
refinement and later polish without turning the codebase into a pile of special
cases.

The roadmap is organized around four goals:

1. Observe what the machine and network are doing.
2. Explain why a connection, socket, or host looks suspicious.
3. Act on a problem safely and deliberately.
4. Keep the UI fast, readable, and stable in narrow terminals.

The phase task lists live under `taklists/` and are designed to stay small
enough to implement, verify, and commit one task at a time.

## Shipped Phases

- Phase 8: Network Observability Foundation
- Phase 9: Socket And Bandwidth Depth
- Phase 10: Safe Network Actions
- Phase 11: Diagnostics And Correlation
- Phase 12: Polish, Automation, And Release Hardening

## Phase 13: Network Overview Refinement

Turn the Network panel into a real operator overview instead of a summary line:

- show each interface with state, carrier, addresses, and current rates
- surface default-route, DNS, and active-interface consistency clearly
- add totals and live traffic summaries that help answer "what is moving?"
- keep VPN and tunnel visibility explicit instead of implied
- preserve clean clipping and fallback states in narrow terminals

## Phase 14: Connections Refinement

Turn the Connections panel into a serious drill-down instead of a raw list:

- show protocol, state, endpoints, ownership, and activity in one readable table
- make IPv4 and IPv6 endpoints easy to scan without inventing hidden detail
- highlight established, listener, and unusual flows without fake risk scoring
- add a focused detail view for the selected connection
- keep empty states and narrow-terminal clipping explicit and stable

## Future Phases

Any later phase numbers will follow the same pattern: ship the code, verify it
locally, update the docs, and keep the task list small enough to review one
item at a time.

## Operating Rule

Do not promote a feature from roadmap to shipped behavior until it has:

- an implementation
- a local verification path
- documentation that matches the implementation
- a task list item that has been completed and reviewed
