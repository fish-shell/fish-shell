#!/usr/bin/env python3
from pexpect_helper import SpawnedProc
import re
import os
import sys

# Disable under SAN - keeps failing because the timing is too tight
if "FISH_CI_SAN" in os.environ:
    sys.exit(0)

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

sendline("echo cat dog")
expect_prompt("cat dog")

sendline("eval (string replace dog tiger -- $history[1])")
expect_prompt("cat tiger")

sendline("eval (string replace dog tiger -- $history[1])")
expect_re(
    "fish: The call stack limit has been exceeded.*"
    + "\r\nin command substitution"
    + "\r\nfish: Unable to evaluate string substitution"
    + re.escape("\r\neval (string replace dog tiger -- $history[1])")
    + "\r\n *\\^~+\\^\\w*"
)
expect_prompt()

sendline("\x04")  # <c-d>
sys.exit(0)
