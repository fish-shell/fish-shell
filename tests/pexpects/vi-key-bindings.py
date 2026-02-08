#!/usr/bin/env python3
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

sendline("fish_vi_key_bindings")
send("echo vim 1234 adsf")
send("\033")
sleep(1)
send(2 * "dge")
sendline("")
expect_re(r"\bvi\b")
expect_prompt()

sendline("echo suggest-this")
send("ech")
send("\033")
sleep(1)
send("li123")
sendline("")
expect_re(r"\bsuggest-thi123s\b")

send("echo hello world")
send("\033")
sleep(1)
send("0wdfw")
sendline("")
expect_re(r"\borld\b")
