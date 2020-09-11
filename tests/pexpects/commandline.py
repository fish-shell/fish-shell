#!/usr/bin/env python3
from pexpect_helper import SpawnedProc

sp = SpawnedProc()
send, sendline, sleep, expect_prompt = sp.send, sp.sendline, sp.sleep, sp.expect_prompt
expect_prompt()

sendline("bind '~' 'handle_tilde'")
expect_prompt()

# printing the current buffer should not remove quoting
sendline(
    "function handle_tilde; echo; echo '@GUARD:1@'; commandline -b; echo '@/GUARD:1@'; commandline -b ''; end"
)
expect_prompt()
sendline("echo \en one \"two three\" four'five six'{7} 'eight~")
expect_prompt("\r\n@GUARD:1@\r\n(.*)\r\n@/GUARD:1@\r\n")

# printing the buffer with -o should remove quoting
sendline(
    "function handle_tilde; echo; echo '@GUARD:2@'; commandline -bo; echo '@/GUARD:2@'; commandline -b ''; end"
)
expect_prompt()
sendline("echo one \"two three\" four'five six'{7} 'eight~")
expect_prompt("\r\n@GUARD:2@\r\n(.*)\r\n@/GUARD:2@\r\n")

# Check that we don't infinitely loop here.
sendline("function fish_mode_prompt; commandline -f repaint; end")
expect_prompt()

sendline("echo foo")
expect_prompt("foo")
