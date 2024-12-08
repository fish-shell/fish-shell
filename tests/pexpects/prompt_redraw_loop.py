#!/usr/bin/env python3
from pexpect_helper import SpawnedProc
import re

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

# Regression test for 9796
# If a process in the prompt exits with a status that would cause a message to be printed,
# don't trigger another prompt execution which would result in an infinite loop.
sendline("sh -c 'kill -ABRT $$'")
expect_prompt(re.escape("fish: Job 1, 'sh -c 'kill -ABRT $$'' terminated by signal SIGABRT (Abort)"))

# Copy current prompt so we can reuse it.
sendline("functions --copy fish_prompt fish_prompt_orig")
expect_prompt()

sendline("function fish_prompt; sh -c 'kill -ABRT $$'; fish_prompt_orig; end")
expect_prompt(re.escape("fish: Job 1, 'sh -c 'kill -ABRT $$'' terminated by signal SIGABRT (Abort)"))

sendline("echo still alive!")
expect_prompt("still alive!")
