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

# Regression test for #7483.
# Ensure we can background a job after a different backgrounded job completes.
sendline("sleep 1")
sleep(0.1)

# ctrl-z - send job to background
send("\x1A")
sleep(0.2)
expect_prompt("has stopped")

# Bring back to fg.
sendline("fg")
sleep(1.0)

# Now sleep is done, right?
expect_prompt()
sendline("jobs")
expect_prompt("jobs: There are no jobs")

# Ensure we can do it again.
sendline("sleep 5")
sleep(0.2)
send("\x1A")
sleep(0.1)
expect_prompt("has stopped")
sendline("fg")
sleep(0.1) # allow tty to transfer
send("\x03")  # control-c to cancel it
