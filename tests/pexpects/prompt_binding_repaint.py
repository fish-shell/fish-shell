#!/usr/bin/env python3
import os
import re

from pexpect_helper import SpawnedProc, control


config = r"""
set -g fish_greeting
set -g fish_autosuggestion_enabled 0
function fish_prompt
    printf 'HEADER\n> '
end
function fish_right_prompt
    printf '12:34:56'
end
function recall_history
    set -l selection "$(command printf '%s\n' ': RECALLED_FIRST' ': RECALLED_SECOND' 'math 31415 + 327')"
    commandline -r -- "$selection"
    commandline -f repaint
end
bind ctrl-r recall_history
bind ctrl-g 'recall_history; commandline -f execute'
"""
env = dict(
    os.environ,
    TERM="xterm-256color",
    TERM_PROGRAM="Apple_Terminal",
    FISH_TEST_NO_RECURRENT_QUERIES="",
)

for binding in ("r", "g"):
    sp = SpawnedProc(env=env, args=["--no-config", "-C", config], dimensions=(30, 120))
    sp.expect_str("\x1b[6n")
    sp.send_cursor_position_report(y=29, x=1)
    sp.send_primary_device_attribute()
    sp.expect_str("12:34:56")

    # An inline history picker can scroll the prompt upward. Wait for its
    # new position before drawing the recalled entry, including on direct
    # execution from the picker.
    sp.send(control(binding))
    sp.expect_str("\x1b[6n")
    assert "RECALLED_FIRST" not in sp.spawn.before, repr(sp.spawn.before)
    assert "31742" not in sp.spawn.before, repr(sp.spawn.before)

    sp.send_cursor_position_report(y=15, x=3)
    sp.send_primary_device_attribute()
    sp.expect_str("\x1b[14;1H")
    assert not re.search(r"\x1b\[\d+;1H", sp.spawn.before), repr(sp.spawn.before)
    sp.expect_str("HEADER\r\n> ")
    sp.expect_str("RECALLED_FIRST")
    if binding == "r":
        sp.send("\r")
    sp.expect_str("31742\r\n")
    sp.spawn.close()
