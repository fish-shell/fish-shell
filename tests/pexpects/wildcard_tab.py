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

# Exclam to clear the commandline.
sendline(r"bind ! 'commandline \'\''")
expect_prompt()

# A do-nothing function to ensure we don't inherit weird completions.
sendline(r"function foo; end")
expect_prompt()

sendline(r"cd (mktemp -d)")
expect_prompt()


# Helper function that sets the commandline to a glob,
# optionally moves the cursor back, tab completes, and then clears the commandline.
def tab_expand_glob(input, expected, move_cursor_back=0):
    send(input)
    if move_cursor_back > 0:
        send("\x1b[D" * move_cursor_back)
    expect_str(input)
    sleep(0.1)
    send("\t")
    expect_str(expected)
    send(r"!")  # clears the commandline


# Don't report tab_expand_glob as the callsite since it is a helper.
tab_expand_glob.callsite_skip = True

sendline(r"touch aaa1 aaa2 aaa3")
expect_prompt()

tab_expand_glob(r"cat *", r"cat aaa1 aaa2 aaa3")
tab_expand_glob(r"cat *2", r"cat aaa2")

# Globs that fail to expand are left alone.
tab_expand_glob(r"cat qqq*", r"cat qqq*")

# Special characters in expanded globs are properly escaped.
sendline(r"touch bb\*bbQ cc\;ccQ")
expect_prompt()
tab_expand_glob(r"cat *Q", r"cat bb\*bbQ cc\;ccQ")

# Cases from #8593.
sendline(r"rm -Rf *; touch README.rst")
expect_prompt()
tab_expand_glob(r"cat R*", r"cat README.rst")

# Glob fails, so offer completion.
tab_expand_glob(r"cat *.r", r"cat *.rst")
tab_expand_glob(r"cat *.rst", r"cat README.rst")

sendline(r"mkdir benchmarks && mkdir benchmarks/somedir && touch benchmarks/somefile")
expect_prompt()

tab_expand_glob(r"echo benchmarks/*", r"echo benchmarks/somedir benchmarks/somefile")

# Trailing slash suppresses files.
# Note we move the cursor backwards one, to right after the glob.
tab_expand_glob(r"echo benchmarks/*/", r"echo benchmarks/somedir/", 1)

# Glob fails so it tries completions which also fails.
# This happens whether the cursor is at the end, or just after the glob.
tab_expand_glob(r"echo ben*/nomatch", r"echo ben*/nomatch")
tab_expand_glob(r"echo ben*/nomatch", r"echo ben*/nomatch", len("/nomatch"))

# Glob fails so it tries completions which succeeds.
tab_expand_glob(r"echo ben*/somed", r"echo ben*/somedir")
tab_expand_glob(r"echo ben*/somed", r"echo ben*/somedir", len("/somed"))

# No glob in "current path component," offer completions.
tab_expand_glob(r"echo {benchmarks/*/,benchm}a", r"echo {benchmarks/*/,benchm}arks/")

# Test undo and redo.
# "<" and ">" to undo and redo respectively.
sendline(r"bind \< undo; bind \> redo")
expect_prompt()

send(r"echo benchmarks/*")
sleep(0.1)
send("\t")
expect_str(r"echo benchmarks/somedir benchmarks/somefile")

# Undo un-expands the command.
send(r"<")
expect_str(r"echo benchmarks/*")

# Redo re-expands it.
send(r">")
expect_str(r"echo benchmarks/somedir benchmarks/somefile")
