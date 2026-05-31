# Trafix TUI Manual Test Checklist

Run these checks before releases and after changes that affect the TUI,
threading, rendering, keyboard handling, or collectors.

## Environment Matrix

- Terminal sizes:
  - `40x12` or smaller: verify the small-terminal fallback.
  - `80x24`: baseline layout.
  - `120x40`: wide layout.
- Terminal capability:
  - Normal color terminal.
  - No-color or limited-color terminal, for example `TERM=dumb`.

## Launch And Exit

- Run `trafix`.
- Confirm the TUI starts without crashing or printing raw escape sequences.
- Press `q`.
- Confirm Trafix exits cleanly and returns the terminal to normal input mode.

## CLI Help And Version

- Run `trafix --help`.
- Confirm help text prints and the TUI does not start.
- Run `trafix --version`.
- Confirm version text prints and the TUI does not start.
- Run `trafix --invalid-option`.
- Confirm an error prints and the TUI does not start.
- Run `trafix diagnostics`.
- Confirm a troubleshooting snapshot prints and the TUI does not start.

## Panel Switching

- Start the TUI.
- Press `c` repeatedly.
- Confirm the primary module changes without flicker, stale data, or broken
  borders.
- Press `l` repeatedly.
- Confirm the support dock cycles through its views without breaking the
  layout.
- Press `t` repeatedly.
- Confirm the top panels hide and show immediately without lag or crashes.
- Resize the terminal while toggling `t`.
- Confirm the layout stays aligned and the dashboard remains responsive.
- Continue switching for at least 30 seconds.
- Confirm CPU usage remains reasonable and the TUI stays responsive.

## Hotkey Popup

- Start the TUI.
- Press `F1`, `h`, and `H`.
- Confirm the hotkey popup opens from each key.
- Confirm the popup lists the keys with short descriptions, including `g`,
  `n`, `v`, `l`, `o`, `a`, `d`, `x`, and `z`.
- Press `Esc`.
- Confirm the popup closes immediately and returns to the dashboard.
- Resize the terminal while the popup is open.
- Confirm the screen redraws cleanly after the popup closes.

## Support Dock Selector

- Start the TUI.
- Press `l`.
- Confirm a modal selector opens for Overview, Logs, Diagnostics, Route/DNS,
  Network Health, Bandwidth Detail, Connection Detail, and Action Audit.
- Use Up/Down to move between items.
- Confirm the selected view changes only after Enter.
- Press `Esc`.
- Confirm the selector closes without changing the active support view.
- Resize to a narrower terminal while the selector is visible.
- Confirm the modal stays stable and clips descriptions safely.

## Diagnostics Views

- Start the TUI.
- Press `g`.
- Confirm the support dock switches to the diagnostics view and shows system,
  network, log, and pressure context.
- Press `n`.
- Confirm the support dock switches to the route and DNS view and shows the
  default route, resolver data, and active interface summary.
- Press `v`.
- Confirm the support dock switches to the network health view and shows CPU,
  memory, disk, process, and network correlation in one compact view.
- Run with missing log access or broken route/DNS data.
- Confirm the support dock shows explicit fallback text instead of blank panes
  or crashes.

## Bandwidth Detail

- Start the TUI on the Network panel.
- Confirm the top talker summary is visible when bandwidth data is available.
- Press `j` and `k`.
- Confirm the selected top talker moves without breaking the layout.
- Press `d` or `Enter`.
- Confirm the support dock switches to the bandwidth detail view for the
  selected top talker.
- Confirm the support dock shows current metadata and recent sample history.
- Press `l`.
- Confirm the support view selector opens and returns cleanly after selection
  or cancel.
- Run in an environment with no meaningful ownership data.
- Confirm the top talker area shows an explicit fallback instead of crashing.

## Action Workflow

- Start the TUI and open the hotkey popup.
- Confirm the popup lists `x`, `z`, `a`, `d`, `g`, `n`, `v`, `l`, and `o`
  alongside the existing controls.
- Press `x`.
- Confirm the process chooser opens, the selected process is visible, and the
  confirmation step appears before any action runs.
- Cancel the action.
- Confirm the dashboard remains intact and the audit popup records the
  cancellation.
- Press `z`.
- Confirm the connection chooser opens with a readable target list.
- Confirm the confirmation step appears and unsupported targets fail cleanly
  instead of crashing or claiming success.
- Press `a`.
- Confirm the support dock shows the recent action trail with readable status
  and message fields.
- Confirm the support dock keeps the dashboard stable while switching views.

## Top Panels Toggle

- Start the TUI with `SHOW_TOP_PANELS = TRUE`.
- Press `t`.
- Confirm the top system, CPU, memory, and disk panels hide and the second row
  expands to the top.
- Press `t` again.
- Confirm the top panels return and the second row moves back down.
- Start the TUI with `SHOW_TOP_PANELS = FALSE`.
- Confirm the top panels start hidden.
- Press `t`.
- Confirm the top panels appear and the layout stays aligned.

## Columns

- Start the TUI.
- Press `c` repeatedly.
- Confirm the primary module changes while the support column remains visible.
- Confirm the dashboard does not grow extra primary columns.
- Confirm the support column repaints cleanly after each switch.
- Set `SHOW_TOP_PANELS = FALSE`.
- Confirm the same primary-module switching works with the top row hidden.
- Confirm the dashboard remains responsive and borders stay intact.

## Two-Column Layout

- Start the TUI on a wide terminal, for example `120x40`.
- Confirm the primary column stays visible and the support column is present.
- Start the TUI on a narrow terminal, for example `80x24`.
- Confirm the support column collapses automatically instead of rendering
  cramped borders.
- Resize between narrow and wide states.
- Confirm the primary column stays readable and the support column repaints
  cleanly when it becomes available.

## Network Views

- Start the TUI.
- Confirm the Network panel shows a compact route, DNS, active interface, and
  VPN summary before the interface-rate list.
- Confirm the Network panel also shows top talker summaries and recent trend
  lines without breaking the border.
- Confirm the Connections panel shows a grouped summary line for protocol,
  state, and ownership before the table.
- Confirm the Socket Inventory panel shows owned sockets with UID, PID,
  process, local, and remote columns.
- Resize the terminal narrower.
- Confirm the summary lines clip instead of breaking the border.

## Pause And Resume

- Press `p`.
- Confirm live values stop changing.
- Press `p` again.
- Confirm live values resume updating.
- While paused, press `q`.
- Confirm Trafix exits cleanly.

## Refresh

- Press `r`.
- Confirm static panels redraw promptly.
- Press `r` multiple times quickly.
- Confirm there is no visible corruption, crash, or terminal lockup.

## Resize

- Start at `80x24`.
- Resize wider and taller.
- Confirm all windows resize and redraw cleanly.
- Resize with `SHOW_TOP_PANELS = FALSE`.
- Confirm the top panels stay hidden and the second row still resizes cleanly.
- Resize narrower and shorter.
- Confirm text is clipped instead of overflowing borders.
- Resize below the minimum supported size.
- Confirm the small-terminal fallback appears.
- Resize back to a normal size.
- Confirm the full dashboard returns.

## Empty And Restricted Data States

- Run as an unprivileged user.
- Confirm connections, socket owners, process, and network panels show readable
  headers and clear empty or unavailable states when data cannot be collected.
- Confirm connection and socket inventory panels report when no visible rows are
  available instead of leaving a blank framed area.
- Confirm the network panel reports missing route, DNS, or interface snapshot
  data clearly when collectors fail or permissions are limited.
- Confirm no panel shows only a blank bordered box when a collector returns no
  rows.

## Color Fallback

- Run with a limited terminal, for example:
  ```sh
  TERM=dumb trafix
  ```
- Confirm Trafix starts or fails gracefully without crashing.
- Confirm the UI remains readable without color.

## Final Sanity

- Run `make`.
- Run `make debug`.
- Run `make asan`.
- Run `make test`.
- Confirm all commands finish without warnings, sanitizer errors, or test
  failures.
