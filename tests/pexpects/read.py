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


def expect_read_prompt():
    expect_re("\r\n?read> $")


def expect_marker(text):
    expect_prompt("\r\n@MARKER:" + str(text) + "@\\r\\n")


def print_var_contents(varname, expected):
    sendline("echo $" + varname)
    expect_prompt(expected)


expect_prompt()

# read

sendline("read")
expect_read_prompt()
sendline("text")
expect_prompt()

sendline("read foo")
expect_read_prompt()
sendline("text")
expect_prompt()
print_var_contents("foo", "text")

sendline("read foo")
expect_read_prompt()
sendline("again\r_marker 1")
expect_prompt()
expect_marker(1)
print_var_contents("foo", "again")

sendline("read foo")
expect_read_prompt()
sendline("bar\r_marker 2")
expect_prompt()
expect_marker(2)
print_var_contents("foo", "bar")

# read -s

sendline("read -s foo")
expect_read_prompt()
sendline("read_s\r_marker 3")
expect_prompt()
expect_marker(3)
print_var_contents("foo", "read_s")

# read -n

sendline("read -n 3 foo")
expect_read_prompt()
sendline("123_marker 3")
expect_prompt()
expect_marker(3)
print_var_contents("foo", "123")

sendline("read -n 3 foo")
expect_read_prompt()
sendline("456_marker 4")
expect_prompt()
expect_marker(4)
print_var_contents("foo", "456")

sendline("read -n 12 foo bar")
expect_read_prompt()
sendline("hello world!_marker 5")
expect_prompt()
expect_marker(5)
print_var_contents("foo", "hello")
print_var_contents("bar", "world!")

sendline("bind ` 'commandline -i test'`")
expect_prompt()
sendline("read -n 4 foo")
expect_read_prompt()
sendline("te`_marker 6")
expect_prompt()
expect_marker(6)
print_var_contents("foo", "tete")

sendline("read -n 4 foo")
expect_read_prompt()
sendline("12`_marker 7")
expect_prompt()
expect_marker(7)
print_var_contents("foo", "12te")

# Verify we don't hang on `read | cat`. See #4540.
sendline("read | cat")
expect_read_prompt()
sendline("bar\r_marker 4540")
expect_prompt()
expect_marker(4540)


# ==========
# The fix for issue #2007 initially introduced a problem when using a function
# to read from /dev/stdin when that is associated with the tty. These tests
# are to ensure we don't introduce a regression.
send("r2l\n")
expect_read_prompt()
send("abc\n")
expect_read_prompt()
send("def\n")
expect_str("abc then def\r\n")
expect_prompt()

# Some systems don't have /dev/stdin - effectively skip the test there.
# I'd love to warn about it, but I don't know how.
send("test -r /dev/stdin; and r2l </dev/stdin; or r2l\n")
expect_read_prompt()
send("ghi\n")
expect_read_prompt()
send("jkl\n")
expect_str("ghi then jkl\r\n")
expect_prompt()
