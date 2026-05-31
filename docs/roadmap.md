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

## Phase 15: Two-Column Operator Layout

Reframe the dashboard into a stable operator surface with one persistent
primary column and one persistent supporting column:

- keep the primary column always visible for overview and drill-down modules
- use the secondary column for logs, diagnostics, audits, and live support data
- preserve the current module content while changing how it is arranged
- hide gracefully on narrow terminals and collapse the supporting column there
- keep the layout readable, fast, and predictable during resize and refresh

## Phase 16: Support View Selector And Detail Dock

Turn the supporting column into a named inspection dock instead of a generic
side panel:

- let the support column host one selected inspection view at a time
- move read-only detail views into the support panel instead of standalone popups
- show a short description for each selectable support item
- keep confirmation and destructive action flows separate from the support panel
- remove stale main-column numbering and keep the help text aligned with the live layout
- preserve narrow-terminal fallback behavior and readable clipping

## Phase 17: Responsive Window Drawing And Cleanup

Make hotkeys feel immediate by tightening redraw, resize, and window lifecycle
paths:

- remove unnecessary window teardown/rebuild work from the `t` hotkey path
- separate layout recomputation from window destruction so state changes are cheap
- make redraw ordering deterministic so borders and content update cleanly
- reduce ncurses lock hold time around high-frequency refresh work
- remove dead and unused code paths that no longer support the shipped UI
- keep the result measurable with fast rebuilds, tests, and sanitizer runs

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
