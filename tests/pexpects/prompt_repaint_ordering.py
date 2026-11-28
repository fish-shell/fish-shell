#!/usr/bin/env python3
import os
import signal

from pexpect_helper import SpawnedProc, control


config = r"""
set -g fish_autosuggestion_enabled 0
function fish_prompt
    printf 'HEADER\n> '
end
function cancel_during_query --on-signal USR1
    commandline -r 'PENDING_REPAINT'
    commandline -f cancel-commandline
    printf 'HANDLED\n'
end
"""
env = dict(
    os.environ,
    TERM="xterm-256color",
    TERM_PROGRAM="Apple_Terminal",
    FISH_TEST_NO_RECURRENT_QUERIES="",
)
sp = SpawnedProc(env=env, args=["--no-config", "-C", config], dimensions=(30, 120))

# Regression test: an event handler can invoke a repaint directly while the
# initial prompt query is pending, bypassing the normal input queue. This must
# be deferred even when no resize has occurred.
sp.expect_str("\x1b[6n")
os.kill(sp.spawn.pid, signal.SIGUSR1)
sp.expect_str("HANDLED\r\n")
assert "PENDING_REPAINT" not in sp.spawn.before, repr(sp.spawn.before)

sp.send_cursor_position_report(y=10, x=1)
sp.send_primary_device_attribute()
sp.expect_str("HEADER\r\n> ")
sp.spawn.close()

# Regression test: a resize during prompt execution defers the repaint. Keep
# that request pending until the resize query completes.
config = r"""
set -g fish_autosuggestion_enabled 0
function fish_prompt
    if set -q shrink
        set -e shrink
        stty cols 100
    end
    printf 'HEADER\n> '
end
function __fish_winch_handler --on-signal WINCH
    commandline -f repaint
end
bind ctrl-g 'set -g shrink 1; commandline -f repaint'
"""
sp = SpawnedProc(env=env, args=["--no-config", "-C", config], dimensions=(30, 120))
sp.expect_str("\x1b[6n")
sp.send_cursor_position_report(y=20, x=1)
sp.send_primary_device_attribute()
sp.expect_str("HEADER\r\n> ")

sp.send(control("g"))
sp.expect_str("\x1b[6n")
sp.send_cursor_position_report(y=21, x=3)
sp.send_primary_device_attribute()
sp.expect_str("HEADER\r\n> ")
sp.spawn.close()

# Regression test: redraw from the known prompt origin after an alternate
# screen restores a right prompt that filled the bottom-right cell.
config = r"""
set -g fish_greeting
set -g fish_autosuggestion_enabled 0
function fish_prompt
    printf 'HEADER\n> '
end
function fish_right_prompt
    printf '12:34:56'
end
bind ctrl-g 'printf "\e[?1049h\e[?1049l"; commandline -f repaint'
"""
sp = SpawnedProc(env=env, args=["--no-config", "-C", config], dimensions=(30, 120))
sp.expect_str("\x1b[6n")
sp.send_cursor_position_report(y=29, x=1)
sp.send_primary_device_attribute()
sp.expect_str("HEADER\r\n> ")
sp.expect_str("\x1b[112C12:34:56")

sp.send(control("g"))
sp.expect_str("\x1b[?1049h\x1b[?1049l")
sp.expect_str("\x1b[29;1H")
assert "HEADER" not in sp.spawn.before, repr(sp.spawn.before)
sp.expect_str("HEADER\r\n> ")
sp.expect_str("\x1b[112C12:34:56")
sp.spawn.close()
