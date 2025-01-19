#!/usr/bin/env python3
from pexpect_helper import SpawnedProc

sp = SpawnedProc(args=["-d", "reader"])
sp.expect_prompt()

# Verify we correctly disable mouse tracking.

# Five char sequence.
sp.send("\x1b[tDE")
sp.expect_str("reader: Disabling mouse tracking")

# Six char sequence.
sp.send("\x1b[MABC")
sp.expect_str("reader: Disabling mouse tracking")

# Nine char sequences.
sp.send("\x1b[TABCDEF")
sp.expect_str("reader: Disabling mouse tracking")

# sleep to catch up under ASAN
sp.sleep(0.5)

# Extended SGR sequences.
sp.send("\x1b[<1;2;3M")
sp.expect_str("reader: Disabling mouse tracking")

sp.send("\x1b[<1;2;3m")
sp.expect_str("reader: Disabling mouse tracking")

sp.sendline("echo done")
sp.expect_prompt("done")
