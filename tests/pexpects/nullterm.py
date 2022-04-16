#!/usr/bin/env python3
import os
from pexpect_helper import SpawnedProc

# Regression test for #8873.
# fish doesn't crash if TERM is unset.
env = os.environ.copy()
env.pop("TERM", None)

sp = SpawnedProc(env=env)
sendline, expect_prompt = sp.sendline, sp.expect_prompt

expect_prompt()
sendline("set_color --italics")
expect_prompt()
sendline("set_color normal")
expect_prompt()
