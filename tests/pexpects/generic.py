#!/usr/bin/env python3
from pexpect_helper import SpawnedProc
import subprocess
import sys
from time import sleep
import os

SpawnedProc()
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

# ensure the Apple key () is typeable
sendline("echo ")
expect_prompt("")

# check that history is returned in the right order (#2028)
# first send 'echo stuff'
sendline("echo stuff")
expect_prompt("stuff")

# last history item should be 'echo stuff'
sendline("echo $history[1]")
expect_prompt("echo stuff")

# last history command should be the one that printed the history
sendline("echo $history[1]")
expect_prompt("echo .history.*")

# Backslashes at end of comments (#1255)
# This backslash should NOT cause the line to continue
sendline("echo -n #comment\\")
expect_prompt()

# a pipe at the end of the line (#1285)
sendline("echo hoge |\n cat")
expect_prompt("hoge")

sendline("echo hoge |    \n cat")
expect_prompt("hoge")

sendline("echo hoge 2>|  \n cat")
expect_prompt("hoge")
sendline("echo hoge >|  \n cat")
expect_prompt("hoge")

sendline("source; or echo failed")
expect_prompt("failed")
