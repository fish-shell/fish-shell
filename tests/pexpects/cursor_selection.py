#!/usr/bin/env python3
from pexpect_helper import SpawnedProc

sp = SpawnedProc()
sendline, expect_prompt, expect_str = sp.sendline, sp.expect_prompt, sp.expect_str
expect_prompt()

# Set up key bindings

# Movement keys from the default key bindings
home, end = "\x01", "\x05"  # Ctrl-A, Ctrl-E
left, right = "\x02", "\x06"  # Ctrl-B, Ctrl-F

# Additional keys to start selecting and dump the current selection
select, dump = "\x00", "!"  # Ctrl-Space, "!"

sendline("bind ctrl-space begin-selection")
expect_prompt()
sendline("bind ! 'echo -n \"<$(commandline --current-selection)>\"'")
expect_prompt()

# Test inclusive mode

sendline("set fish_cursor_selection_mode inclusive")

# at the beginning of the line
sendline("echo" + home + select + dump)
expect_str("<e>")
sendline("echo" + home + select + right + dump)
expect_str("<ec>")
sendline("echo" + home + right + select + left + dump)
expect_str("<ec>")

# in the middle of the line
sendline("echo" + home + right + select + dump)
expect_str("<c>")
sendline("echo" + home + right + select + right + dump)
expect_str("<ch>")
sendline("echo" + home + right + right + select + left + dump)
expect_str("<ch>")

# at the end of the line
sendline("echo" + end + select + dump)
expect_str("<>")
sendline("echo" + end + select + left + dump)
expect_str("<o>")
sendline("echo" + end + left + select + right + dump)
expect_str("<o>")

# Test exclusive mode

sendline("set fish_cursor_selection_mode exclusive")

# at the beginning of the line
sendline("echo" + home + select + dump)
expect_str("<>")
sendline("echo" + home + select + right + dump)
expect_str("<e>")
sendline("echo" + home + right + select + left + dump)
expect_str("<e>")

# in the middle of the line
sendline("echo" + home + right + select + dump)
expect_str("<>")
sendline("echo" + home + right + select + right + dump)
expect_str("<c>")
sendline("echo" + home + right + right + select + left + dump)
expect_str("<c>")

# at the end of the line
sendline("echo" + end + select + dump)
expect_str("<>")
sendline("echo" + end + select + left + dump)
expect_str("<o>")
sendline("echo" + end + left + select + right + dump)
expect_str("<o>")

# Test default setting.
# We only test that the correct implementation is chosen and rely on the detailed tests from above.

# without a configured selection mode
sendline("set -u fish_cursor_selection_mode")
sendline("echo" + home + right + select + right + dump)
expect_str("<c>")

# with an unknown setting
sendline("set fish_cursor_selection_mode unknown")
sendline("echo" + home + right + select + right + dump)
expect_str("<c>")
