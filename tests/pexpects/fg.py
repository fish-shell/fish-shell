#!/usr/bin/env python3
from pexpect_helper import SpawnedProc
import subprocess
import sys
import time

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

sendline("sleep 500")
sendline("set -l foo bar; echo $foo")
expect_str("")
sleep(0.2)

# ctrl-z - send job to background
send("\x1A")
sleep(0.2)
expect_prompt()
sendline("set -l foo bar; echo $foo")
expect_str("bar")

expect_prompt()
sendline("fg")
expect_str("Send job 1, 'sleep 500' to foreground")
sleep(0.2)
sendline("set -l foo bar; echo $foo")
expect_str("")
# ctrl-c - cancel
send("\x03")

expect_prompt()
sendline("set -l foo bar; echo $foo")
expect_str("bar")
