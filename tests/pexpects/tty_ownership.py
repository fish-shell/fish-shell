#!/usr/bin/env python3

# SPDX-FileCopyrightText: © 2020 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

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
