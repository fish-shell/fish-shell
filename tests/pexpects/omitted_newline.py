#!/usr/bin/env python3
import sys
import os

sys.path.append("..")

from pexpect_helper import SpawnedProc

env = os.environ.copy()
env["fish_omitted_newline_char"] = "¶"

sp = SpawnedProc(env=env)

sendline, expect_str = (
    sp.sendline,
    sp.expect_str,
)

expect_str("Welcome to fish")

sendline("printf foo")

# Match separately because terminal escape sequences
# may appear between output and newline indicator
expect_str("foo")
expect_str("¶")
