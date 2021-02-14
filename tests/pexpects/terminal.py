#!/usr/bin/env python3
from pexpect_helper import SpawnedProc

# Set a 0 terminal size
sp = SpawnedProc(args=["-d", "term-support"], dimensions=(0,0))
send, sendline, sleep, expect_prompt, expect_re, expect_str = (
    sp.send,
    sp.sendline,
    sp.sleep,
    sp.expect_prompt,
    sp.expect_re,
    sp.expect_str,
)

expect_prompt()
# Now confirm it defaulted to 80x24
sendline("echo $COLUMNS $LINES")
expect_str("80 24")
expect_str("term-support: Terminal has 0 columns, falling back to default width")
expect_str("term-support: Terminal has 0 rows, falling back to default height")
expect_prompt()

sendline("stty -a")
expect_prompt()
# Confirm flow control in the shell is disabled - we should ignore the ctrl-s in there.
sendline("echo hello\x13hello")
expect_prompt("hellohello")

# Confirm it is off for external commands
sendline("stty -a | string match -q '*-ixon -ixoff*'; echo $status")
expect_prompt("0")

# Turn flow control on
sendline("stty ixon ixoff")
expect_prompt()
sendline("stty -a | string match -q '*ixon ixoff*'; echo $status")
expect_prompt("0")

# Confirm flow control in the shell is disabled - we should ignore the ctrl-s in there.
sendline("echo hello\x13hello")
# This should not match because we should not get any output.
# Unfortunately we have to wait for the timeout to expire - set it to a second.
expect_str("hellohello", timeout=1, shouldfail=True)
send("\x11") # ctrl-q to resume flow
expect_prompt("hellohello")
