#!/usr/bin/env python3
from pexpect_helper import SpawnedProc
import subprocess
import sys
import time

sp = SpawnedProc()
send, sendline, sleep, expect_prompt, expect_re = (
    sp.send,
    sp.sendline,
    sp.sleep,
    sp.expect_prompt,
    sp.expect_re,
)
expect_prompt()

# Verify that if we attempt to exit with a job in the background we get warned
# about that job and are told to type `exit` a second time.
send("sleep 111 &\r")
expect_prompt()
send("exit\r")
expect_re(
    """There are still jobs active:\r
\r
   PID  Command\r
 *\\d+  sleep 111 &\r
\r
A second attempt to exit will terminate them.\r
Use 'disown PID' to remove jobs from the list without terminating them.\r"""
)
expect_prompt()

# Running anything other than `exit` should result in the same warning with
# the shell still running.
send("sleep 113 &\r")
expect_prompt()
send("exit\r")
expect_re(
    """There are still jobs active:\r
\r
   PID  Command\r
 *\\d+  sleep 113 &\r
 *\\d+  sleep 111 &\r
\r
A second attempt to exit will terminate them.\r
Use 'disown PID' to remove jobs from the list without terminating them.\r"""
)
expect_prompt()

# Verify that asking to exit a second time does so.
send("exit\r")

for t in range(0,3):
    proc = subprocess.run(
        ["pgrep", "-l", "-f", "sleep 11"], stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    if proc.returncode != 0:
        break
    # This is cheesy, but on Travis with thread-sanitizer this can be slow enough that the process is still running, so we sleep for a bit.
    time.sleep(1)
else:
    print("Commands still running")
    print(proc.stdout)
    sys.exit(1)
