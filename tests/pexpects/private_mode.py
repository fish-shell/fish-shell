#!/usr/bin/env python3
import os
import time
from pexpect_helper import SpawnedProc

sp = SpawnedProc()
sendline, sleep, expect_prompt, expect_str = (
    sp.sendline,
    sp.sleep,
    sp.expect_prompt,
    sp.expect_str,
)

# Helper to sendline and add to our view of history.
recorded_history = []
private_mode_active = False
fish_path = os.environ.get("fish")


# Send a line and record it in our history array if private mode is not active.
def sendline_record(s):
    sendline(s)
    if not private_mode_active:
        recorded_history.append(s)


expect_prompt()

# Start off with no history.
sendline(r" builtin history clear; builtin history save")
expect_prompt()

# Ensure that fish_private_mode can be changed - see #7589.
sendline_record(r"echo before_private_mode")
expect_prompt("before_private_mode")
sendline(r" builtin history save")
expect_prompt()

# Enter private mode.
sendline_record(r"set -g fish_private_mode 1")
expect_prompt()
private_mode_active = True

sendline_record(r"echo check2 $fish_private_mode")
expect_prompt("check2 1")

# Nothing else gets added.
sendline_record(r"true")
expect_prompt()
sendline_record(r"false")
expect_prompt()

# Leave private mode. The command to leave it is still private.
sendline_record(r"set -ge fish_private_mode")
expect_prompt()
private_mode_active = False

# New commands get added.
sendline_record(r"set alpha beta")
expect_prompt()

# Check our history is what we expect.
# We have to wait for the time to tick over, else our item risks being discarded.
now = time.time()
start = int(now)
while now - start < 1:
    sleep(now - start)
    now = time.time()

sendline(r" builtin history save ; $fish -c 'string join \n -- $history'")
expect_prompt("\r\n".join(reversed(recorded_history)))
