#!/usr/bin/env python3
from pexpect_helper import SpawnedProc

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
