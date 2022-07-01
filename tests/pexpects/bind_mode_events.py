#!/usr/bin/env python3
from pexpect_helper import SpawnedProc
import os
import signal

sp = SpawnedProc()
send, sendline, sleep, expect_prompt = sp.send, sp.sendline, sp.sleep, sp.expect_prompt
expect_prompt()

send("set -g fish_key_bindings fish_vi_key_bindings\r")
expect_prompt()

send("echo ready to go\r")
expect_prompt("\r\nready to go\r\n")
send(
    "function add_change --on-variable fish_bind_mode ; set -g MODE_CHANGES $MODE_CHANGES $fish_bind_mode ; end\r"
)
expect_prompt()

# normal mode
send("\033")
sleep(0.050)

# insert mode
send("i")
sleep(0.050)

# back to normal mode
send("\033")
sleep(0.050)

# insert mode again
send("i")
sleep(0.050)

send("echo mode changes: $MODE_CHANGES\r")
expect_prompt("\r\nmode changes: default insert default insert\r\n")

# Regression test for #8125.
# Control-C should return us to insert mode.
send("set -e MODE_CHANGES\r")
expect_prompt()

timeout = 0.15

if "CI" in os.environ:
    # This doesn't work under TSan, because TSan prevents select() being
    # interrupted by a signal.
    import sys

    print("SKIPPING the last of bind_mode_events.py")
    sys.exit(0)

# Put some text on the command line and then go back to normal mode.
send("echo stuff")
sp.expect_str("echo stuff")
send("\033")
sleep(timeout)

os.kill(sp.spawn.pid, signal.SIGINT)
sleep(timeout)

# We should be back in insert mode now.
send("echo mode changes: $MODE_CHANGES\r")
expect_prompt("\r\nmode changes: default insert\r\n")
