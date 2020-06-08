#!/usr/bin/env python3
from pexpect_helper import SpawnedProc

sp = SpawnedProc()
sendline, expect_prompt = sp.sendline, sp.expect_prompt

expect_prompt()
sendline("function echo_wrap ; /bin/echo $argv ; sleep 0.1; end")
expect_prompt()

for i in range(5):
    sendline(
        "echo_wrap 1 2 3 4 | $fish_test_helper become_foreground_then_print_stderr ; or exit 1"
    )
    expect_prompt("become_foreground_then_print_stderr done")

# 'not' because we expect to have no jobs, in which case `jobs` will return false
sendline("not jobs")
expect_prompt("jobs: There are no jobs")

sendline("function inner ; command true ; end; function outer; inner; end")
expect_prompt()
for i in range(5):
    sendline(
        "outer | $fish_test_helper become_foreground_then_print_stderr ; or exit 1"
    )
    expect_prompt("become_foreground_then_print_stderr done")

sendline("not jobs")
expect_prompt("jobs: There are no jobs", unmatched="Should be no jobs")
