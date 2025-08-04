#!/usr/bin/env python3
import pexpect
import re
from pexpect_helper import SpawnedProc

def strip_ansi(s):
    return re.sub(r'\x1b\].*?\x07|\x1b\[?[0-9;]*[A-Za-z]', '', s)

def normalize_line(line):
    return re.sub(r'\s+', ' ', line.replace('\t', ' ').strip())

sp = SpawnedProc()
send, sendline, sleep, expect_prompt, expect_re, expect_str = (
    sp.send,
    sp.sendline,
    sp.sleep,
    sp.expect_prompt,
    sp.expect_re,
    sp.expect_str,
)
expect_prompt(timeout=5)
# SETUP: Reset shell state and set up environment
sendline("bind --erase --all")
expect_prompt(timeout=5)
sendline("set -e -g fish_key_bindings fish_prompt")
expect_prompt(timeout=5)
sendline("set TERM xterm")
expect_prompt(timeout=5)
sendline(r"bind ! 'commandline \"\"'")
expect_prompt(timeout=5)
sendline("set -g fish_pager_completion_threshold 1")
expect_prompt(timeout=5)
sendline("set -gx MY_SUPER_SECRET 'secret_value'")
expect_prompt(timeout=5)
sendline("set -gx MY_OTHER_SECRET 'another_secret'")
expect_prompt(timeout=5)
sendline("set -gx MY_PUBLIC_VALUE 'public_value'")
expect_prompt(timeout=5)
sendline("set -gx MY_OTHER_VALUE 'other_value'")
expect_prompt(timeout=5)
sendline("set -gx MY_NORMAL_VAR 'normal_value'")
expect_prompt(timeout=5)
sendline("set -g fish_censor_vars '*_SECRET' MY_PUBLIC_VALUE")
expect_prompt(timeout=5)
# ACTION: Trigger variable completions with a single tab
send("echo $MY_\t")
sleep(0.05)
try:
    sp.spawn.expect(pexpect.TIMEOUT, timeout=0.5)
except pexpect.TIMEOUT:
    pass
# VERIFY: Check pager output for censored and non-censored variables
stripped_output = strip_ansi(sp.spawn.before)
lines = [normalize_line(line) for line in stripped_output.splitlines() if line.strip() and '(Variable:' in line]
expected_patterns = [
    "$MY_SUPER_SECRET (Variable: [censored])",
    "$MY_OTHER_SECRET (Variable: [censored])",
    "$MY_PUBLIC_VALUE (Variable: [censored])",
    "$MY_OTHER_VALUE (Variable: other_value)",
    "$MY_NORMAL_VAR (Variable: normal_value)"
]
censored_super_secret = False
censored_other_secret = False
censored_public = False
non_censored_other = False
non_censored_normal = False
for line in lines:
    if "$MY_SUPER_SECRET" in line:
        assert "(Variable: [censored])" in line, f"Censored description not found in line: {line}"
        censored_super_secret = True
        print(f"DEBUG: Censored MY_SUPER_SECRET match found in line: {line}")
    if "$MY_OTHER_SECRET" in line:
        assert "(Variable: [censored])" in line, f"Censored description not found in line: {line}"
        censored_other_secret = True
        print(f"DEBUG: Censored MY_OTHER_SECRET match found in line: {line}")
    if "$MY_PUBLIC_VALUE" in line:
        assert "(Variable: [censored])" in line, f"Censored description not found in line: {line}"
        censored_public = True
        print(f"DEBUG: Censored MY_PUBLIC_VALUE match found in line: {line}")
    if "$MY_OTHER_VALUE" in line:
        assert "(Variable: other_value)" in line, f"Non-censored description not found in line: {line}"
        non_censored_other = True
        print(f"DEBUG: Non-censored MY_OTHER_VALUE match found in line: {line}")
    if "$MY_NORMAL_VAR" in line:
        assert "(Variable: normal_value)" in line, f"Non-censored description not found in line: {line}"
        non_censored_normal = True
        print(f"DEBUG: Non-censored MY_NORMAL_VAR match found in line: {line}")
assert censored_super_secret, "Censored variable $MY_SUPER_SECRET not found in output"
assert censored_other_secret, "Censored variable $MY_OTHER_SECRET not found in output"
assert censored_public, "Censored variable $MY_PUBLIC_VALUE not found in output"
assert non_censored_other, "Non-censored variable $MY_OTHER_VALUE not found in output"
assert non_censored_normal, "Non-censored variable $MY_NORMAL_VAR not found in output"
# CLEANUP: Dismiss pager and clear command line
send("q")
sleep(0.5)
send("\x03")
sleep(0.1)
send("\r")
sleep(0.1)
send("!")
expect_prompt(timeout=5)
sendline("set -e fish_pager_completion_threshold MY_SUPER_SECRET MY_OTHER_SECRET MY_PUBLIC_VALUE MY_OTHER_VALUE MY_NORMAL_VAR fish_censor_vars")
expect_prompt(timeout=5)
