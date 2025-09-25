#!/usr/bin/env python3
from pexpect_helper import SpawnedProc

sp = SpawnedProc()
sendline, expect_prompt = sp.sendline, sp.expect_prompt

expect_prompt()
sendline("status job-control full")
expect_prompt()

sendline(
    "$fish -c 'status job-control full ; $fish_test_helper report_foreground' &; wait"
)
expect_prompt()

sendline("echo it worked")
expect_prompt("it worked")

# Regression test for #9181
sendline("status job-control interactive")
expect_prompt()
sendline("$fish_test_helper abandon_tty")
expect_prompt()
sendline("echo cool")
expect_prompt("cool")
sendline("true ($fish_test_helper abandon_tty)")
expect_prompt()
sendline("echo even cooler")
expect_prompt("even cooler")

sendline("TERM=not-dumb $fish -c read")
sp.send_primary_device_attribute()
sendline("something")
sp.expect_re(r"\x1b\[\?u")
expect_prompt()
