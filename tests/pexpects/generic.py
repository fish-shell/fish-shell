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

# ensure the Apple key () is typeable
sendline("echo \xf8ff")
expect_prompt("\xf8ff")

# check that history is returned in the right order (#2028)
# first send 'echo stuff'
sendline("echo stuff")
expect_prompt("stuff")

# last history item should be 'echo stuff'
sendline("echo $history[1]")
expect_prompt("echo stuff")

# last history command should be the one that printed the history
sendline("echo $history[1]")
expect_prompt("echo \\$history\\[1\\]")

# Backslashes at end of comments (#1255)
# This backslash should NOT cause the line to continue
sendline("echo -n #comment\\")
expect_prompt()

# a pipe at the end of the line (#1285)
sendline("echo hoge |\n cat")
expect_prompt("hoge")

sendline("echo hoge |    \n cat")
expect_prompt("hoge")

sendline("echo hoge 2>|  \n cat")
expect_prompt("hoge")
sendline("echo hoge >|  \n cat")
expect_prompt("hoge")

sendline("$fish --no-execute 2>&1")
expect_prompt("error: no-execute mode enabled and no script given. Exiting")

sendline("source; or echo failed")
expect_prompt("failed")

# See that `type` tells us the function was defined interactively.
sendline("function foo; end; type foo")
expect_str("foo is a function with definition\r\n")
expect_str("# Defined interactively\r\n")
expect_str("function foo")
expect_str("end")
expect_prompt()

# See that `functions` terminates
sendline("functions")
expect_re(".*fish_prompt,")
expect_prompt()
