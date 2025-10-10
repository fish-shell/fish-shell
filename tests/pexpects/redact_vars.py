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
# Test that `fish_redact_vars` correctly redacts variable values in pager completions,
# including default patterns and explicit variable names.
#

# Test variables for default redaction patterns
sendline("set -gx MY_API_KEY 'api_key_value'")
expect_prompt()
sendline("set -gx MY_TOKEN 'token_value'")
expect_prompt()
sendline("set -gx MY_SECRET 'secret_value'")
expect_prompt()

# Test variables for explicit redaction and non-redacted cases
sendline("set -gx MY_EXPLICIT_REDACTION 'explicit_value'")
expect_prompt()
sendline("set -gx MY_NORMAL_VAR 'normal_value'")
expect_prompt()

# Append explicit redaction to default set
sendline("set -ga fish_redact_vars MY_EXPLICIT_REDACTION")
expect_prompt()

# Trigger variable completion to open the pager
send("echo $MY_\t")
sleep(0.05)

# Wait for pager to draw and flush output
try:
    sp.spawn.expect(pexpect.TIMEOUT, timeout=0.5)
except pexpect.TIMEOUT:
    pass

# Process pager output to verify redaction status
stripped_output = re.sub(r"\x1b\].*?\x07|\x1b\[?[0-9;]*[A-Za-z]", "", sp.spawn.before)
output_text = "\n".join(
    [
        re.sub(r"\s+", " ", line.replace("\t", " ").strip())
        for line in stripped_output.splitlines()
        if line.strip() and "(Variable:" in line
    ]
)

# Verify expected redactions
assert "$MY_API_KEY (Variable: [redacted])" in output_text
assert "$MY_TOKEN (Variable: [redacted])" in output_text
assert "$MY_SECRET (Variable: [redacted])" in output_text
assert "$MY_EXPLICIT_REDACTION (Variable: [redacted])" in output_text
assert "$MY_NORMAL_VAR (Variable: normal_value)" in output_text
