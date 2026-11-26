#!/usr/bin/env python3
import os
import signal

from pexpect_helper import SpawnedProc


config = r'''
set -g fish_autosuggestion_enabled 0
function fish_prompt
    printf 'HEADER\n> '
end
function cancel_during_query --on-signal USR1
    commandline -r 'PENDING_REPAINT'
    commandline -f cancel-commandline
    printf 'HANDLED\n'
end
'''
env = dict(
    os.environ,
    TERM='xterm-256color',
    TERM_PROGRAM='Apple_Terminal',
    FISH_TEST_NO_RECURRENT_QUERIES='',
)
sp = SpawnedProc(env=env, args=['--no-config', '-C', config], dimensions=(30, 120))

# Regression test: an event handler can invoke a repaint directly while the
# initial prompt query is pending, bypassing the normal input queue. This must
# be deferred even when no resize has occurred.
sp.expect_str('\x1b[6n')
os.kill(sp.spawn.pid, signal.SIGUSR1)
sp.expect_str('HANDLED\r\n')
assert 'PENDING_REPAINT' not in sp.spawn.before, repr(sp.spawn.before)

sp.send_cursor_position_report(y=10, x=1)
sp.send_primary_device_attribute()
sp.expect_str('HEADER\r\n> ')
sp.spawn.close()
