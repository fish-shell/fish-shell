#!/usr/bin/env python3

# SPDX-FileCopyrightText: © 2020 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

from pexpect_helper import SpawnedProc
import platform
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

# Hack: NetBSD's sleep likes quitting when waking up
# (but also does so under /bin/sh)
testproc = "sleep 500" if platform.system() != "NetBSD" else "cat"
sendline(testproc)
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
expect_str("Send job 1 (" + testproc + ") to foreground")
sleep(0.2)
sendline("set -l foo bar; echo $foo")
expect_str("")
# ctrl-c - cancel
send("\x03")

expect_prompt()
sendline("set -l foo bar; echo $foo")
expect_str("bar")

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
sleep(0.1)  # allow tty to transfer
send("\x03")  # control-c to cancel it

expect_prompt()
sendline("jobs")
expect_prompt("jobs: There are no jobs")

# Regression test for #2214: foregrounding from a key binding works!
sendline(r"bind \cr 'fg >/dev/null 2>/dev/null'")
expect_prompt()
sendline("$fish_test_helper print_stop_cont")
sleep(0.2)

send("\x1A")  # ctrl-z
expect_prompt("SIGTSTP")
sleep(0.1)
send("\x12")  # ctrl-r, placing fth in foreground
expect_str("SIGCONT")

# Do it again.
send("\x1A")
expect_str("SIGTSTP")
sleep(0.1)
send("\x12")
expect_str("SIGCONT")

# End fth by sending it anything.
send("\x12")
sendline("derp")
expect_prompt()
