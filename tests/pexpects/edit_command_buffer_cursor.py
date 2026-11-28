#!/usr/bin/env python3
import os

from pexpect_helper import SpawnedProc, control


config = r"""
set -g fish_greeting
set -g fish_cursor_insert line
set -gx VISUAL test_editor
function test_editor
    printf '\e[2 q'
    return "$editor_status"
end
set -g editor_status 0
function fish_prompt
    printf 'EDITOR> '
end
fish_vi_key_bindings
function report_mode_change --on-variable fish_bind_mode
    printf 'MODE CHANGED\n'
end
bind -M insert ctrl-g 'commandline -r "echo edited"'
bind -M insert ctrl-t 'set -g editor_status 7; edit_command_buffer; printf "EXIT:%s\n" "$status"'
"""

sp = SpawnedProc(
    env=dict(os.environ, TERM="xterm-256color"), args=["--no-config", "-C", config]
)
sp.expect_str("EDITOR> ")
sp.send(control("g"))
sp.expect_str("echo edited")

# The stock binding should restore the insert cursor without setting the mode.
sp.send("\x1be")
sp.expect_str("\x1b[2 q")
sp.expect_str("EDITOR> ")
assert "\x1b[6 q" in sp.spawn.before, repr(sp.spawn.before)
assert "MODE CHANGED" not in sp.spawn.before, repr(sp.spawn.before)

# A failed editor also changes the cursor. Restore it and preserve the status.
sp.send(control("t"))
sp.expect_str("\x1b[2 q")
sp.expect_str("EXIT:7")
assert "\x1b[6 q" in sp.spawn.before, repr(sp.spawn.before)
assert "MODE CHANGED" not in sp.spawn.before, repr(sp.spawn.before)
sp.spawn.close()
