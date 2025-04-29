#!/usr/bin/env python3
from pexpect_helper import SpawnedProc

import os

env = os.environ.copy()
env["TERM"] = "foot"

sp = SpawnedProc(env=env)
send, sendline, sleep, expect_prompt, expect_re, expect_str = (
    sp.send,
    sp.sendline,
    sp.sleep,
    sp.expect_prompt,
    sp.expect_re,
    sp.expect_str,
)
expect_prompt()

sendline("function check; and echo true; or echo false; end")

sendline("status xtgettcap am; check")
expect_str("\x1bP+q616d\x1b\\") # 616d is "am" in hex
send("\x1bP1+r616d\x1b\\") # success
sp.send_primary_device_attribute()
expect_str("true")

sendline("status xtgettcap an; check")
expect_str("\x1bP+q616e\x1b\\") # 616e is "an" in hex
send("\x1bP0+r616e\x1b\\") # failure
sp.send_primary_device_attribute()
expect_str("false")
