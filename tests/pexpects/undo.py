#!/usr/bin/env python3
from pexpect_helper import SpawnedProc

sp = SpawnedProc()
send, sendline, sleep, expect_prompt, expect_re, expect_str = (
    sp.send,
    sp.sendline,
    sp.sleep,
    sp.expect_prompt,
    sp.expect_re,
    sp.expect_str,
)
expect_prompt()

sendline("bind Undo undo; bind Redo redo")
expect_prompt()

send("echo word")
expect_str("echo word")
expect_str("echo word")  # Not sure why we get this twice.

send("Undo")
expect_str("echo")

send("Redo")
expect_str("echo word")
