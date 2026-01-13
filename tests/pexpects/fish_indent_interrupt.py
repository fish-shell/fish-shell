#!/usr/bin/env python3
from pexpect_helper import SpawnedProc
import os, sys

if "CI" in os.environ:
    sys.exit(127)

sp = SpawnedProc(timeout=2)
send, sendline, sleep, expect_prompt = (
    sp.send,
    sp.sendline,
    sp.sleep,
    sp.expect_prompt,
)

# Verify that the fish_indent builtin can be interrupted with ctrl-c.
expect_prompt()
sendline("fish_indent")
sleep(0.1)
send("\x03")  # ctrl-c
expect_prompt()
