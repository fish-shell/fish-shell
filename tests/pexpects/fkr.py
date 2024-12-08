#!/usr/bin/env python3
from pexpect_helper import SpawnedProc
from time import sleep
import os

os.environ["fish_escape_delay_ms"] = "10"
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

sendline("$fish_key_reader --version")
expect_re("fish_key_reader, version .*")
expect_prompt()

sendline("exec $fish_key_reader -c -V")

# Do we get the expected startup prompt?
expect_str("Press a key:")

# Is a single control char echoed correctly?
send("\x07")
expect_str("# decoded from: \\x07\r\n")
expect_str("bind ctrl-g 'do something'\r\n")

# Is a non-ASCII UTF-8 sequence prefaced by an escape char handled correctly?
sleep(0.020)
send("\x1B")
expect_str("# decoded from: \\e\r\n")
expect_str("bind escape 'do something'\r\n")
send("ö")
expect_str("# decoded from: ö\r\n")
expect_str("bind ö 'do something'\r\n")
send("\u1234")
expect_str("bind ሴ 'do something'\r\n")

# Is a NULL char echoed correctly?
sleep(0.020)
send("\x00")
expect_str("bind ctrl-space 'do something'\r\n")

send("\x1b\x7f")
expect_str("# decoded from: \\e\\x7f\r\n")
expect_str("bind alt-backspace 'do something'\r\n")

send("\x1c")
expect_str(r"bind ctrl-\\ 'do something'")

# Does it keep running if handed control sequences in the wrong order?
send("\x03")
sleep(0.010)
send("\x04")

# Now send a second ctrl-d. Does that terminate the process like it should?
sleep(0.050)
send("\x04\x04")
expect_str("Exiting at your request.\r\n")
