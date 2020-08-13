#!/usr/bin/env python3
from pexpect_helper import SpawnedProc

sp = SpawnedProc()
sendline, sleep, expect_prompt, expect_str = (
    sp.sendline,
    sp.sleep,
    sp.expect_prompt,
    sp.expect_str,
)

# Ensure that if child processes SIGINT, we exit our loops.
# This is an interactive test because the parser is expected to
# recover from SIGINT in interactive mode.
# Test for #7259.

expect_prompt()
sendline("while true; sh -c 'echo Here we go; sleep .25; kill -s INT $$'; end")
sleep(0.30)
expect_str("Here we go")
expect_prompt()

sendline("echo it worked")
expect_str("it worked")
expect_prompt()
