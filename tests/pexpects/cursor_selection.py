#!/usr/bin/env python3
from pexpect_helper import SpawnedProc

home, end = "\x01", "\x05"
left, right ="\x02", "\x06"
select, dump = "\x00", "!"

sp = SpawnedProc()
sendline, expect_prompt, expect_str = sp.sendline, sp.expect_prompt, sp.expect_str
expect_prompt()

sendline("bind -k nul begin-selection")
expect_prompt()
sendline("bind ! 'echo -n \"<$(commandline --current-selection)>\"'")
expect_prompt()

sendline("set fish_cursor_selection_mode inclusive")

sendline("echo" + home + select + dump)
expect_str("<e>")
sendline("echo" + home + select + right + dump)
expect_str("<ec>")
sendline("echo" + home + right + select + left + dump)
expect_str("<ec>")

sendline("echo" + home + right + select + dump)
expect_str("<c>")
sendline("echo" + home + right + select + right + dump)
expect_str("<ch>")
sendline("echo" + home + right + right + select + left + dump)
expect_str("<ch>")

sendline("echo" + end + select + dump)
expect_str("<>")
sendline("echo" + end + select + left + dump)
expect_str("<o>")
sendline("echo" + end + left + select + right + dump)
expect_str("<o>")

sendline("set fish_cursor_selection_mode exclusive")

sendline("echo" + home + select + dump)
expect_str("<>")
sendline("echo" + home + select + right + dump)
expect_str("<e>")
sendline("echo" + home + right + select + left + dump)
expect_str("<e>")

sendline("echo" + home + right + select + dump)
expect_str("<>")
sendline("echo" + home + right + select + right + dump)
expect_str("<c>")
sendline("echo" + home + right + right + select + left + dump)
expect_str("<c>")

sendline("echo" + end + select + dump)
expect_str("<>")
sendline("echo" + end + select + left + dump)
expect_str("<o>")
sendline("echo" + end + left + select + right + dump)
expect_str("<o>")
