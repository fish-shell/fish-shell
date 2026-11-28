#!/usr/bin/env python3
from pexpect_helper import SpawnedProc, control
import os

env = os.environ.copy()
env["TERM"] = "not-dumb"
env["FISH_TEST_NO_RECURRENT_QUERIES"] = ""

sp = SpawnedProc(env=env, scroll_content_up_supported=True)
sendline, expect_prompt = sp.sendline, sp.expect_prompt
sp.expect_str("\x1b[6n")
sp.send_cursor_position_report(y=1, x=1)
sp.send_primary_device_attribute()
expect_prompt()

sendline("bind ctrl-g scrollback-push")
sp.expect_str("\x1b[6n")
sp.send_cursor_position_report(y=10, x=1)
sp.send_primary_device_attribute()
expect_prompt()
sp.send(control("g"))
sp.expect_str("\x1b[9S\x1b[9A")

sp.send("\r")
sp.expect_str("\x1b[6n")
sp.send_cursor_position_report(y=15, x=1)
sp.send_primary_device_attribute()
expect_prompt()
sp.send(control("l"))
sp.expect_str("\x1b[14S\x1b[14A")
