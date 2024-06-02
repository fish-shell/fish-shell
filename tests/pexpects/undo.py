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
expect_prompt()
sendline("bind U,n,d,o undo; bind R,e,d,o redo")
expect_prompt()

send("echo word")
expect_str("echo word")

send("Undo")
expect_str("echo")

send("Redo\r")
expect_prompt("echo word")

sendline("bind x begin-undo-group 'commandline -i \"+ \"' 'commandline -i 3' end-undo-group")
expect_prompt()
send("math 2 x\r")
expect_prompt("5")

send("math 4x")
send("Undo1\r")
expect_prompt("41")

send("math 5 x")
send("\x02") # c-b, moving before the 3
send("Undo")
send("Redo")
send("9\r") # 5 + 93
expect_prompt("98")
