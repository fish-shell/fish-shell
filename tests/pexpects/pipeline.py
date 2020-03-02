#!/usr/bin/env python3
from pexpect_helper import SpawnedProc

sp = SpawnedProc()
sp.expect_prompt()
sp.sendline("function echo_wrap ; /bin/echo $argv ; sleep 0.1; end")
sp.expect_prompt()

for i in range(5):
    sp.sendline(
        "echo_wrap 1 2 3 4 | $fish_test_helper become_foreground_then_print_stderr ; or exit 1"
    )
    sp.expect_prompt("become_foreground_then_print_stderr done")

# 'not' because we expect to have no jobs, in which case `jobs` will return false
sp.sendline("not jobs")
sp.expect_prompt("jobs: There are no jobs")

sp.sendline("function inner ; command true ; end; function outer; inner; end")
sp.expect_prompt()
for i in range(5):
    sp.sendline(
        "outer | $fish_test_helper become_foreground_then_print_stderr ; or exit 1"
    )
    sp.expect_prompt("become_foreground_then_print_stderr done")

sp.sendline("not jobs")
sp.expect_prompt("jobs: There are no jobs", unmatched="Should be no jobs")
