#!/usr/bin/env python3
from pexpect_helper import SpawnedProc
import platform

import os
import sys

# Disable under SAN - keeps failing because the timing is too tight
if "FISH_CI_SAN" in os.environ:
    sys.exit(0)


# Set a 0 terminal size
sp = SpawnedProc(args=["-d", "term-support"], dimensions=(0, 0))
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

# See if $LINES/$COLUMNS change in response to sigwinch, also in a --on-signal function
sendline("function on-winch --on-signal winch; echo $LINES $COLUMNS; end")
expect_prompt()
sleep(4)
sp.spawn.setwinsize(40, 50)
expect_str("40 50")
sendline("echo $LINES $COLUMNS")
expect_prompt("40 50")

sp.spawn.setwinsize(20, 70)
expect_str("20 70")
sendline("echo $LINES $COLUMNS")
expect_prompt("20 70")

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

# TODO
import sys
sys.exit(0)
# HACK: This fails on FreeBSD, macOS and NetBSD for some reason, maybe
# a pexpect issue?
# So disable it everywhere but linux for now.
if platform.system() in ["Linux"]:
    # Flow control does not work in CSI u mode, but it works while we are running an external process.
    sendline("sh -c 'for i in $(seq 10); do echo $i; sleep 1; done")
    sendline("hello\x13hello")
    # This should not match because we should not get any output.
    # Unfortunately we have to wait for the timeout to expire - set it to a second.
    expect_str("hellohello", timeout=1, shouldfail=True)
    send("\x11")  # ctrl-q to resume flow
    expect_prompt("hellohello")
