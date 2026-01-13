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
expect_prompt()

sendline("read")
send("12a3" + "\033")
sleep(1)
sendline("h" + "x")
expect_str("123")
expect_prompt()

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

send("echo abcdef")
send("\033")
sleep(1)
send("hhhdhldl")
sendline("")
expect_re(r"\bacef\b")

# Test x copying to clipboard
send("echo hello")
send("\033")
sleep(1)
# Delete 'o', then 'l', then 'l', buffer contains 'l'
send("xxx")
# Re-insert 'l'
send("p")
sendline("")
# Results in 'hel' because 'p' pastes after the cursor ('e').
# string is 'he' + paste 'l' after 'e' = 'hel'
expect_re(r"\bhel\b")

send("echo 123456")
send("\033")
sleep(1)
send("hhh")  # cursor on 3
send("3x")  # deletes 3, 4, 5
send("p")  # pastes after 2 -> 123456
sendline("")
expect_re(r"\b123456\b")
