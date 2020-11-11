#!/usr/bin/env python3
from pexpect_helper import SpawnedProc
import subprocess
import sys
from time import sleep
import os

os.environ["fish_escape_delay_ms"] = "10"
SpawnedProc()
sp = SpawnedProc()
send, sendline, sleep, expect_prompt, expect_re, expect_str = (
    sp.send,
    sp.sendline,
    sp.sleep,
    sp.expect_prompt,
    sp.expect_re,
    sp.expect_str,
)
expect_prompt()

sendline("exec $fish_key_reader -c")

# Do we get the expected startup prompt?
expect_str("Press a key:")

# Is a single control char echoed correctly?
send("\x07")
expect_str("char: \\cG  (or \\a)\r\nbind \\a 'do something'\r\n")

# Is a non-ASCII UTF-8 sequence prefaced by an escape char handled correctly?
sleep(0.020)
# send "\x1B\xE1\x88\xB4"
send("\x1B\u1234")
expect_str("char: \\u1234\r\nbind \\e\\u1234 'do something'\r\n")

# Is a NULL char echoed correctly?
sleep(0.020)
send("\x00")
expect_str("char: \\c@\r\nbind -k nul 'do something'\r\n")

# Does it keep running if handed control sequences in the wrong order?
send("\x03")
sleep(0.010)
send("\x04")
expect_str("char: \\cD\r\n")

# Now send a second [ctrl-D]. Does that terminate the process like it should?
sleep(0.050)
send("\x04\x04")
expect_str("char: \\cD\r\n")
expect_str("Exiting at your request.\r\n")
