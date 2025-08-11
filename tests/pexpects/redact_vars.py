#!/usr/bin/env python3
import os
import re

import pexpect
from pexpect_helper import SpawnedProc

env = os.environ.copy()
env["TERM"] = "xterm"

sp = SpawnedProc(env=env)
send, sendline, sleep, expect_prompt, expect_re, expect_str = (
    sp.send,
    sp.sendline,
    sp.sleep,
    sp.expect_prompt,
    sp.expect_re,
    sp.expect_str,
)

expect_prompt()

#
# Test that `fish_redact_vars` correctly redacts variable values in pager completions.
#

sendline("set -gx MY_SUPER_SECRET 'secret_value'")
expect_prompt()
sendline("set -gx MY_OTHER_SECRET 'another_secret'")
expect_prompt()
sendline("set -gx MY_EXPLICIT_SECRET 'public_value'")
expect_prompt()
sendline("set -gx MY_OTHER_VALUE 'other_value'")
expect_prompt()
sendline("set -gx MY_NORMAL_VAR 'normal_value'")
expect_prompt()

sendline("set -g fish_redact_vars '*_SECRET' MY_EXPLICIT_SECRET")
expect_prompt()

# Trigger variable completion to open the pager.
send("echo $MY_\t")
sleep(0.05)

# Wait for a short timeout to allow the pager to draw and flush its
# output to the pexpect buffer. We expect a TIMEOUT.
try:
    sp.spawn.expect(pexpect.TIMEOUT, timeout=0.5)
except pexpect.TIMEOUT:
    pass

# Verify the pager output shows the correct redaction status for each variable.
stripped_output = re.sub(r"\x1b\].*?\x07|\x1b\[?[0-9;]*[A-Za-z]", "", sp.spawn.before)
output_text = "\n".join(
    [
        re.sub(r"\s+", " ", line.replace("\t", " ").strip())
        for line in stripped_output.splitlines()
        if line.strip() and "(Variable:" in line
    ]
)

assert "$MY_SUPER_SECRET (Variable: [redacted])" in output_text
assert "$MY_OTHER_SECRET (Variable: [redacted])" in output_text
assert "$MY_EXPLICIT_SECRET (Variable: [redacted])" in output_text
assert "$MY_OTHER_VALUE (Variable: other_value)" in output_text
assert "$MY_NORMAL_VAR (Variable: normal_value)" in output_text
