#!/usr/bin/env python3
from pexpect_helper import SpawnedProc

# Set a 0 terminal size
sp = SpawnedProc(args=["-d", "term-support"], dimensions=(0,0))
sendline, expect_prompt, expect_str = sp.sendline, sp.expect_prompt, sp.expect_str

expect_prompt()
# Now confirm it defaulted to 80x24
sendline("echo $COLUMNS $LINES")
expect_str("80 24")
expect_str("term-support: Terminal has 0 columns, falling back to default width")
expect_str("term-support: Terminal has 0 rows, falling back to default height")
expect_prompt()

