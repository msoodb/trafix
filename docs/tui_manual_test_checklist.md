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

## Panel Switching

- Start the TUI.
- Press `1`, `2`, and `3` repeatedly.
- Confirm each visible second-row slot switches panels without flicker, stale data,
  or broken borders.
- Continue switching for at least 30 seconds.
- Confirm CPU usage remains reasonable and the TUI stays responsive.

## Hotkey Popup

- Start the TUI.
- Press `F1`, `h`, and `H`.
- Confirm the hotkey popup opens from each key.
- Press `Esc`.
- Confirm the popup closes immediately and returns to the dashboard.
- Resize the terminal while the popup is open.
- Confirm the screen redraws cleanly after the popup closes.

## Columns

- Start the TUI.
- Press `c` repeatedly.
- Confirm the number of second-row columns cycles through the supported layouts.
- Confirm panel borders align after every layout change.
- Confirm active panel workers keep updating after each layout change.
- Set `SHOW_TOP_PANELS = FALSE`.
- Confirm the second row stays aligned and fills the available space.
- Cycle columns again with the top row hidden.
- Confirm the dashboard remains responsive and borders stay intact.

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
