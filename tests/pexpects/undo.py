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

sendline("set -g fish_autosuggestion_enabled 0")
sendline("bind U,n,d,o undo; bind R,e,d,o redo")
expect_prompt()

send("echo word")
expect_str("echo word")
expect_str("echo word")
expect_str("echo word")

send("Undo")
expect_str("echo")

send("Redo")
expect_str("echo word")
