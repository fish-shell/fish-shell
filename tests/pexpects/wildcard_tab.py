#!/usr/bin/env python3
import signal
from pexpect_helper import SpawnedProc

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

sendline("cd (mktemp -d)")
expect_prompt()

sendline("touch aaa1 aaa2 aaa3")
expect_prompt()

send("cat *")
sleep(0.1)
send("\t")
expect_str("cat aaa1 aaa2 aaa3")
sendline("")
expect_prompt()

send("cat *2")
sleep(0.1)
send("\t")
expect_str("cat aaa2")
sendline("")
expect_prompt()

# Globs that fail to expand are left alone.
send("cat qqq*")
sleep(0.1)
send("\t")
expect_str("cat qqq*")
sendline("")
expect_prompt()

# Special characters in expanded globs are properly escaped.
sendline(r"touch bb\*bbQ cc\;ccQ")
expect_prompt()
send("cat *Q")
sleep(0.1)
send("\t")
expect_str(r"cat bb\*bbQ cc\;ccQ")
sendline("")
expect_prompt()
