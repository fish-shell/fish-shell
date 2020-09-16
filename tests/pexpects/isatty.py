#!/usr/bin/env python3
from pexpect_helper import SpawnedProc
import subprocess
import sys
import time

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

sendline("test -t 0; echo $status")
expect_prompt("0")

sendline("""function t
test -t 0 && echo stdin
test -t 1 && echo stdout
test -t 2 && echo stderr
end""")
expect_prompt()

sendline("t")
expect_str("stdin")
expect_str("stdout")
expect_str("stderr")
expect_prompt()

sendline("cat </dev/null | t")
expect_str("stdout")
expect_str("stderr")
expect_prompt()

sendline("t | cat")
expect_str("stdin")
expect_str("stderr")
expect_prompt()

sendline("t 2>| cat")
expect_str("stdin")
expect_str("stdout")
expect_prompt()

sendline("cat </dev/null | t | cat")
expect_str("stderr")
expect_prompt()
sendline("cat </dev/null | t 2>| cat")
expect_str("stdout")
expect_prompt()

sendline("t </dev/null")
expect_str("stdout")
expect_str("stderr")
expect_prompt()

sendline("isatty stdin && echo yes")
expect_str("yes")
expect_prompt()

sendline("cat </dev/null | isatty stdin || echo no")
expect_str("no")
expect_prompt()
