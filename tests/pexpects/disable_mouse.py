#!/usr/bin/env python3
from pexpect_helper import SpawnedProc
import subprocess
import sys
import time

sp = SpawnedProc(args=["-d", "reader"])
sp.expect_prompt()

# Verify we correctly diable mouse tracking.

# Five char sequence.
sp.send("\x1b[tDE")
sp.expect_str("reader: Disabling mouse tracking")

# Six char sequence.
sp.send("\x1b[MABC")
sp.expect_str("reader: Disabling mouse tracking")

# Nine char sequences.
sp.send("\x1b[TABCDEF")
sp.expect_str("reader: Disabling mouse tracking")

# Extended SGR sequences.
sp.send("\x1b[<fooM")
sp.expect_str("reader: Disabling mouse tracking")

sp.send("\x1b[<foobarm")
sp.expect_str("reader: Disabling mouse tracking")

sp.sendline("echo done")
sp.expect_prompt("done")
