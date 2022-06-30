#!/usr/bin/env python3
from pexpect_helper import SpawnedProc

import re

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

# ? prints the command line, bracketed by <>, then clears it.
sendline(r"""bind '?' 'echo "<$(commandline)>"; commandline ""'; """)
expect_prompt()

# Basic test.
sendline(r"abbr alpha beta")
expect_prompt()

send(r"alpha ?")
expect_str(r"<beta >")

# Default abbreviations expand only in command position.
send(r"echo alpha ?")
expect_str(r"<echo alpha >")

# Abbreviation expansions may have multiple words.
sendline(r"abbr --add emacsnw emacs -nw")
expect_prompt()
send(r"emacsnw ?")
expect_str(r"<emacs -nw >")

# Regression test that abbreviations still expand in incomplete positions.
sendline(r"""abbr --erase (abbr --list)""")
expect_prompt()
sendline(r"""abbr --add foo echo bar""")
expect_prompt()
sendline(r"if foo")
expect_str(r"if echo bar")
sendline(r"end")
expect_prompt(r"bar")

# Regression test that 'echo (' doesn't hang.
sendline(r"echo )")
expect_str(r"Unexpected ')' for unopened parenthesis")
send(r"?")
expect_str(r"<echo )>")
