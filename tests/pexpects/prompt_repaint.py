#!/usr/bin/env python3
import os

from pexpect_helper import SpawnedProc, control


config = r"""
set -g fish_autosuggestion_enabled 0
function fish_prompt
    printf 'header\n> '
end
function fish_right_prompt
    printf '12:34:56'
end
function repaint_after_alternate_screen
    printf '\e[?1049h\e[?1049l'
    commandline -f repaint
end
bind ctrl-g repaint_after_alternate_screen
"""

# Terminal.app can duplicate a multiline prompt after restoring the alternate
# screen with a right prompt in the bottom-right cell. Check the redraw's cursor
# addressing; an ordinary PTY cannot reproduce Terminal.app's rendering bug.
for terminal, row in [
    ("Apple_Terminal", 29),
    ("Apple_Terminal", 7),
    ("Apple_Terminal", None),
    ("other-terminal", 29),
]:
    env = os.environ.copy()
    env["TERM"] = "xterm-256color"
    env["TERM_PROGRAM"] = terminal
    env["FISH_TEST_NO_RECURRENT_QUERIES"] = "1" if row is None else ""
    sp = SpawnedProc(env=env, args=["-C", config], dimensions=(30, 120))

    if row is not None:
        sp.expect_str("\x1b[6n")
        sp.send_cursor_position_report(y=row, x=1)
        sp.send_primary_device_attribute()
    sp.expect_str("header\r\n> ")
    sp.expect_str("12:34:56")

    sp.send(control("g"))
    sp.expect_str("\x1b[?1049h\x1b[?1049l")
    if terminal == "Apple_Terminal" and row is not None:
        sp.expect_str(f"\x1b[{row};1H")
    else:
        sp.expect_str("\x1b[A")
    sp.expect_str("header\r\n> ")
    sp.expect_str("12:34:56")
    sp.spawn.close()
