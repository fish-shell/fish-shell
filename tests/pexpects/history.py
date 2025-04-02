#!/usr/bin/env python3
# This is a very fragile test. Sorry about that. But interactively entering
# commands and verifying they are recorded correctly in the interactive
# history and that history can be manipulated is inherently difficult.
#
# This is meant to verify just a few of the most basic behaviors of the
# interactive history to hopefully keep regressions from happening. It is not
# meant to be a comprehensive test of the history subsystem. Those types of
# tests belong in the src/fish_tests.cpp module.
#

# The history function might pipe output through the user's pager. We don't
# want something like `less` to complicate matters so force the use of `cat`.
from pexpect_helper import SpawnedProc
import os

env = os.environ.copy()
env["PAGER"] = "cat"

sp = SpawnedProc(env=env)

send, sendline, sleep, expect_prompt, expect_re, expect_str = (
    sp.send,
    sp.sendline,
    sp.sleep,
    sp.expect_prompt,
    sp.expect_re,
    sp.expect_str,
)
expect_prompt()

# ==========
# Start by ensuring we're not affected by earlier tests. Clear the history.
sendline("builtin history clear")
expect_prompt()

# ==========
# The following tests verify the behavior of the history builtin.
# ==========

# ==========
# List our history which should be empty after just clearing it.
sendline("echo start1; builtin history; echo end1")
expect_prompt("start1\r\nend1\r\n")

# Our history should now contain the previous command and nothing else.
sendline("echo start2; builtin history; echo end2")
expect_prompt("start2\r\necho start1; builtin history; echo end1\r\nend2\r\n")

# ==========
# The following tests verify the behavior of the history function.
# ==========

# ==========
# Verify explicit searching for the first two commands in the previous tests
# returns the expected results.
sendline("history search --reverse 'echo start' | cat")
expect_prompt("echo start1;.*\r\necho start2;")

# ==========
# Verify searching is the implicit action.
sendline("history -p 'echo start'")
expect_prompt("echo start2.*\r\necho start1")

# ==========
# Verify searching with a request for timestamps includes the timestamps.
sendline("history search --show-time='# %F %T%n' --prefix 'echo start'")
expect_prompt(
    "# \\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\r\necho start2; .*\r\n# \\d\\d\\d\\d-\\d\\d-\\d\\d \\d\\d:\\d\\d:\\d\\d\r\necho start1;"
)

# ==========
# Verify explicit searching for an exact command returns just that command.
# returns the expected results.
sendline("echo hello")
expect_prompt()
sendline("echo goodbye")
expect_prompt()
sendline("echo hello again")
expect_prompt()
sendline("echo hello AGAIN")
expect_prompt()

sendline("history search --exact 'echo goodbye' | cat")
expect_prompt("echo goodbye\r\n")

sendline("history search --exact 'echo hello' | cat")
expect_prompt("echo hello\r\n")

# This is slightly subtle in that it shouldn't actually match anything between
# the command we sent and the next prompt.
sendline("history search --exact 'echo hell' | cat")
expect_prompt("history search --exact 'echo hell' | cat\r\n")

# Verify that glob searching works.
sendline("history search --prefix 'echo start*echo end' | cat")
expect_prompt("echo start1; builtin history; echo end1\r\n")

# ==========
# Delete a single command we recently ran.
sendline("history delete -e -C 'echo hello'")
expect_prompt()
sendline("echo count hello (history search -e -C 'echo hello' | count)")
expect_prompt("count hello 0\r\n")

# ==========
# Interactively delete one of multiple matched commands. This verifies that we
# delete the first entry matched by the prefix search (the most recent command
# sent above that matches).
sendline("history delete -p 'echo hello'")
expect_re("history delete -p 'echo hello'\r\n")
expect_re(r"\[1\] echo hello AGAIN\r\n")
expect_re(r"\[2\] echo hello again\r\n\r\n")
expect_re(
    "Enter nothing to cancel the delete, or\r\nEnter one or more of the entry IDs or ranges like '5..12', separated by a space.\r\nFor example '7 10..15 35 788..812'.\r\nEnter 'all' to delete all the matching entries.\r\n"
)
expect_re("Delete which entries\? ")
sendline("1")
expect_prompt('Deleting history entry 1: "echo hello AGAIN"\r\n')

# Verify that the deleted history entry is gone and the other one that matched
# the prefix search above is still there.
sendline("echo count AGAIN (history search -e -C 'echo hello AGAIN' | count)")
expect_prompt("count AGAIN 0\r\n")

sendline("echo count again (history search -e -C 'echo hello again' | count)")
expect_prompt("count again 1\r\n")

# Verify that the $history var has the expected content.
sendline("echo history2=$history[2]")
expect_prompt("history2=echo count AGAIN .*\r\n")

# Verify that history search is case-insensitive by default
sendline("echo term")
expect_prompt("term")
sendline("echo TERM")
expect_prompt("TERM")
sendline("echo banana")
expect_prompt("banana")
send("ter\x1b[A")  # up-arrow
expect_re("echo TERM")
sendline("")
expect_prompt("TERM")

# Check that leading space makes an ephemeral item
sendline(" echo ephemeral")
expect_prompt("ephemeral")
send("\x1b[A")  # up-arrow
expect_re(" echo ephemeral")
sendline("")
expect_prompt("ephemeral")
sendline(" ")
expect_prompt()
send("\x1b[A")
expect_re("echo TERM")  # not ephemeral!

# Verify that clear-session works as expected
# Note: This test depends on that history merge resets the session from history clear-sessions point of view.
sendline("builtin history clear")
expect_prompt()

# create before history
sendline("echo before1")
expect_prompt()
sendline("echo before2")
expect_prompt()
# "reset" session with history merge
sendline("history merge")
expect_prompt()
# create after history
sendline("echo after")
expect_prompt()
# clear session
sendline("history clear-session")
expect_prompt()
sendline("history search --exact 'echo after' | cat")
expect_prompt("\r\n")

# Check history filtering
# We store anything that starts with "echo ephemeral".
sendline(
    "function fish_should_add_to_history; string match -q 'echo ephemeral*' -- $argv; and return 2; return 0; end"
)
expect_prompt("")
# Check that matching the line works
# (fish_should_add_to_history is itself stored in history so we match "ephemeral!" to avoid it)
sendline("echo ephemeral! line")
expect_prompt("ephemeral! line")
sendline("echo nonephemeral! line")
expect_prompt("nonephemeral! line")
sendline("true")
expect_prompt()
sendline("echo a; history search '*ephemeral!*' | cat; echo b")
expect_prompt(r"a\r\n.*echo nonephemeral! line\r\nb\r\n")

# If fish_should_add_to_history exists, it will completely take over,
# so even lines with spaces are stored
sendline(" echo spaced")
expect_prompt("spaced")
sendline("true")
expect_prompt()
sendline("echo a; history search '*spaced*' | cat; echo b")
expect_prompt("a\r\n.* echo spaced\r\nb\r\n")
