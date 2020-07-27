#!/usr/bin/env python3
from pexpect_helper import SpawnedProc

sp = SpawnedProc()
sendline, expect_prompt = sp.sendline, sp.expect_prompt

expect_prompt()
sendline("status job-control full")
expect_prompt()

sendline("$fish -c 'status job-control full ; $fish_test_helper report_foreground' &; wait")
expect_prompt()

sendline("echo it worked")
expect_prompt("it worked")
