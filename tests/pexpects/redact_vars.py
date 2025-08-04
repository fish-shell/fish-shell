#!/usr/bin/env python3
import pexpect
import re
import os
from pexpect_helper import SpawnedProc

# Set TERM in the environment before spawning fish. This is more robust
# and ensures the shell initializes with the correct terminal type.
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

# Start with a clean prompt.
expect_prompt()

#
# Test that `fish_redact_vars` correctly redacts variable values in pager completions.
#

# Reset key bindings to ensure a clean test environment.
sendline("bind --erase --all")
expect_prompt()

# Set a low pager threshold to force it to appear for our list of completions.
sendline("set -g fish_pager_completion_threshold 1")
expect_prompt()

# Set up some variables, some of which we will configure for redaction.
sendline("set -gx MY_SUPER_SECRET 'secret_value'")
expect_prompt()
sendline("set -gx MY_OTHER_SECRET 'another_secret'")
expect_prompt()

# This one will be redacted explicitly by name.
sendline("set -gx MY_PUBLIC_VALUE 'public_value'")
expect_prompt()

# This one should remain visible.
sendline("set -gx MY_OTHER_VALUE 'other_value'")
expect_prompt()

sendline("set -gx MY_NORMAL_VAR 'normal_value'")
expect_prompt()

# Configure the redaction rules: a wildcard for `*_SECRET` and one explicit name.
sendline("set -g fish_redact_vars '*_SECRET' MY_PUBLIC_VALUE")
expect_prompt()

# Trigger variable completion to open the pager.
send("echo $MY_\t")
sleep(0.05)

# The pager display and subsequent command-line redraw can create a race condition.
# A short, non-deterministic wait allows all output to flush to the pexpect buffer.
# We then grab the entire buffer post-facto for verification.
try:
    sp.spawn.expect(pexpect.TIMEOUT, timeout=0.5)
except pexpect.TIMEOUT:
    pass

# Verify that the pager output shows the correct redaction status for each variable.
# We process the output and then run direct assertions on the resulting text.
stripped_output = re.sub(r'\x1b\].*?\x07|\x1b\[?[0-9;]*[A-Za-z]', '', sp.spawn.before)
# Join all relevant lines into a single string to check for presence.
output_text = "\n".join([re.sub(r'\s+', ' ', line.replace('\t', ' ').strip()) for line in stripped_output.splitlines() if line.strip() and '(Variable:' in line])

# Assert that each expected line exists somewhere in the captured output.
assert "$MY_SUPER_SECRET (Variable: [redacted])" in output_text
assert "$MY_OTHER_SECRET (Variable: [redacted])" in output_text
assert "$MY_PUBLIC_VALUE (Variable: [redacted])" in output_text
assert "$MY_OTHER_VALUE (Variable: other_value)" in output_text
assert "$MY_NORMAL_VAR (Variable: normal_value)" in output_text

# Clean up after the test.
# Since the first completion is already on the command line, the most reliable
# way to get a clean prompt is to simply execute the command by sending Enter.
send("\r")
# This will execute 'echo $MY_NORMAL_VAR', so we expect its output followed by a prompt.
expect_prompt("normal_value")

# Erase the variables and settings created for the test.
sendline("set -e fish_pager_completion_threshold MY_SUPER_SECRET MY_OTHER_SECRET MY_PUBLIC_VALUE MY_OTHER_VALUE MY_NORMAL_VAR fish_redact_vars")
expect_prompt()
