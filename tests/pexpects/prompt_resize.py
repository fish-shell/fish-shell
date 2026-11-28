#!/usr/bin/env python3
import os

from pexpect_helper import SpawnedProc, control


prompt_header = "HEADER " + "0123456789" * 7


def spawn_prompt(*, header=prompt_header):
    config = rf"""
set -g fish_autosuggestion_enabled 0
set -g fish_handle_reflow 1
function __fish_winch_handler --on-signal WINCH
    if test "$fish_handle_reflow" = 1
        commandline -f repaint >/dev/null 2>/dev/null
    end
end
function resize_observed --on-signal WINCH
    if test "$COLUMNS" = 40
        printf '\e]0;resize-observed\a'
    end
end
function fish_prompt
    printf '{header}\n> '
end
function fish_right_prompt
    printf '12:34:56'
end
"""
    env = dict(
        os.environ,
        TERM="xterm-256color",
        TERM_PROGRAM="Apple_Terminal",
        FISH_TEST_NO_RECURRENT_QUERIES="",
    )
    sp = SpawnedProc(env=env, args=["-C", config], dimensions=(30, 120))
    sp.expect_str("\x1b[6n")
    sp.send_cursor_position_report(y=20, x=1)
    sp.send_primary_device_attribute()
    sp.expect_str("12:34:56")
    return sp


# Regression test: the terminal reflows the old 77-column header onto four rows
# when shrinking to 20 columns. Its cursor report still identifies the same
# header origin, rather than the final physical row of that header.
sp = spawn_prompt()
for width, cursor_row in [(20, 24), (120, 21)]:
    sp.spawn.setwinsize(30, width)
    sp.expect_str("\x1b[6n")
    sp.send_cursor_position_report(y=cursor_row, x=3)
    sp.send_primary_device_attribute()
    sp.expect_str("12:34:56")
    assert "\x1b[20;1H" in sp.spawn.before, repr(sp.spawn.before)
sp.spawn.close()

# Regression test: reflow can leave the start of the header above the viewport.
# Keep that prefix untouched until growing the window restores its first row.
sp = spawn_prompt()
sp.spawn.setwinsize(3, 20)
sp.expect_str("\x1b[6n")
sp.send_cursor_position_report(y=3, x=3)
sp.send_primary_device_attribute()
sp.expect_str("12:34:56")
assert "HEADER" not in sp.spawn.before, repr(sp.spawn.before)
assert "…" not in sp.spawn.before, repr(sp.spawn.before)

sp.spawn.setwinsize(30, 120)
sp.expect_str("\x1b[6n")
sp.send_cursor_position_report(y=21, x=3)
sp.send_primary_device_attribute()
sp.expect_str("12:34:56")
assert "\x1b[20;1H" in sp.spawn.before, repr(sp.spawn.before)
assert sp.spawn.before.count("HEADER") == 1, repr(sp.spawn.before)
sp.spawn.close()

# Regression test: a one-row window hides the header. Growing the window
# restores those rows above the cursor, so the redraw must include their offset.
for prompt_lines in [2, 3]:
    header = "\n".join(f"HEADER{i}" for i in range(prompt_lines - 1))
    sp = spawn_prompt(header=header)
    sp.spawn.setwinsize(1, 120)
    sp.expect_str("\x1b[6n")
    sp.send_cursor_position_report(y=1, x=3)
    sp.send_primary_device_attribute()
    sp.expect_str("12:34:56")

    sp.spawn.setwinsize(30, 120)
    sp.expect_str("\x1b[6n")
    sp.send_cursor_position_report(y=20 + prompt_lines - 1, x=3)
    sp.send_primary_device_attribute()
    sp.expect_str("12:34:56")
    assert "\x1b[20;1H" in sp.spawn.before, repr(sp.spawn.before)
    sp.spawn.close()

# Regression test: resizing while a height query is pending invalidates its
# reply, including a reply received before the query's terminating response.
# The second resize reflows the header, moving the cursor from row 21 to row 24.
for reply_before_resize in [False, True]:
    sp = spawn_prompt()
    sp.spawn.setwinsize(29, 120)
    sp.expect_str("\x1b[6n")
    if reply_before_resize:
        sp.send_cursor_position_report(y=21, x=3)
    sp.spawn.setwinsize(29, 20)
    if not reply_before_resize:
        sp.send_cursor_position_report(y=21, x=3)
    sp.send_primary_device_attribute()

    sp.expect_str("\x1b[6n")
    assert "\x1b[21;1H" not in sp.spawn.before, repr(sp.spawn.before)
    sp.send_cursor_position_report(y=24, x=3)
    sp.send_primary_device_attribute()
    sp.expect_str("12:34:56")
    assert "\x1b[20;1H" in sp.spawn.before, repr(sp.spawn.before)

    sp.spawn.setwinsize(30, 120)
    sp.expect_str("\x1b[6n")
    sp.send_cursor_position_report(y=21, x=3)
    sp.send_primary_device_attribute()
    sp.expect_str("12:34:56")
    assert "\x1b[20;1H" in sp.spawn.before, repr(sp.spawn.before)
    sp.send("X")
    sp.expect_str("X")
    sp.spawn.close()

# Regression test: interrupting a query also discards queued resize events.
# A second resize must not leave painting permanently suspended after Ctrl-C.
sp = spawn_prompt()
sp.spawn.setwinsize(30, 20)
sp.expect_str("\x1b[6n")
sp.spawn.setwinsize(30, 40)
# Wait until the second resize has reached Fish before interrupting its query.
sp.expect_str("\x1b]0;resize-observed\x07")
sp.send(control("c"))
sp.send("X")
sp.expect_str("X")
sp.spawn.close()
