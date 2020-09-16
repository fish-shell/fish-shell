#!/usr/bin/env python3
from pexpect_helper import SpawnedProc
import subprocess
import sys
import time
import os

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

# See that --print-colors prints the colors colored.
# Note that we don't check *all* of them, just a few.
sendline("set_color --print-colors")
expect_str("black")
expect_str("blue")
expect_str("brblack")
expect_str("brblue")
expect_str("brcyan")
expect_str("brgreen")
expect_str("brmagenta")
expect_str("brred")
expect_str("brwhite")
expect_str("bryellow")
expect_str("cyan")
expect_str("green")
expect_str("magenta")
expect_str("\x1b[31mred")
expect_str("\x1b[37mwhite")
expect_str("\x1b[33myellow")
expect_str("normal")
expect_prompt()

sendline ("set_color --background blue --print-colors")
expect_str("black")
expect_str("blue")
expect_str("brblack")
expect_str("brblue")
expect_str("brcyan")
expect_str("brgreen")
expect_str("brmagenta")
expect_str("brred")
expect_str("brwhite")
expect_str("bryellow")
expect_str("cyan")
expect_str("green")
expect_str("magenta")
expect_str("\x1b[31m\x1b[44mred")
expect_str("white")
expect_str("yellow")
expect_str("normal")
expect_prompt()

