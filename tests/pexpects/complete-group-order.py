#!/usr/bin/env python3
from pexpect_helper import SpawnedProc

sp = SpawnedProc()
send, sendline, sleep, expect_prompt, expect_re, expect_str = (
    sp.send,
    sp.sendline,
    sp.sleep,
    sp.expect_prompt,
    sp.expect_re,
    sp.expect_str,
)

# This test verifies the overall sort order across multiple `complete -k` completions with identical
# predicates. We check for legacy "reverse what you'd expect" order, as discussed in #9221.

expect_prompt()
sendline("function fooc; true; end;")
expect_prompt()

# A non-`complete -k` completion
sendline('complete -c fooc -fa "alpha delta bravo"')
expect_prompt()

# A `complete -k` completion chronologically and alphabetically before the next completion. You'd
# expect it to come first, but we documented that it will come second.
sendline('complete -c fooc -fka "golf charlie echo"')
expect_prompt()

# A `complete -k` completion that is chronologically after and alphabetically after the previous
# one, so a naive sort would place it after but we want to make sure it comes before.
sendline('complete -c fooc -fka "india foxtrot hotel"')
expect_prompt()

# Another non-`complete -k` completion
sendline('complete -c fooc -fa "kilo juliett lima"')
expect_prompt()

sendline("set TERM xterm")
expect_prompt()

# Generate completions
send("fooc \t")

expect_re(
    "india\\W+hotel\\W+charlie\\W+alpha\\W+delta\\W+kilo\r\n"
    + "foxtrot\\W+golf\\W+echo\\W+bravo\\W+juliett\\W+lima"
)
