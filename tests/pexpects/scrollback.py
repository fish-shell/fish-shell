#!/usr/bin/env python3
from pexpect_helper import SpawnedProc, control
import os

env = os.environ.copy()
env["TERM"] = "not-dumb"

sp = SpawnedProc(env=env, scroll_up_content_supported=True)
sendline, expect_prompt = sp.sendline, sp.expect_prompt
expect_prompt()

sendline("bind ctrl-g scrollback-push")
expect_prompt()
sp.send(control("g"))
sp.send_cursor_position_report(y=10, x=5)
sp.send_primary_device_attribute()

sp.expect_str("\x1b[9S\x1b[9A")
