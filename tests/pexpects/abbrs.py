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
# Default abbreviations expand only in command position.
sendline(r"abbr alpha beta")
expect_prompt()

send(r"alpha ?")
expect_str(r"<beta >")

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

# Support position anywhere.
sendline(r"abbr alpha --position anywhere beta2")
expect_prompt()

send(r"alpha ?")
expect_str(r"<beta2 >")

send(r"echo alpha ?")
expect_str(r"<echo beta2 >")

# Support regex.
sendline(r"abbr alpha --regex 'A[0-9]+Z' beta3")
expect_prompt()
send(r"A123Z ?")
expect_str(r"<beta3 >")
send(r"AZ ?")
expect_str(r"<AZ >")
send(r"QA123Z ?")
expect_str(r"<QA123Z >")
send(r"A0000000000000000000009Z ?")
expect_str(r"<beta3 >")

# Support functions. Here anything in between @@ is uppercased, except 'nope'.
sendline(
    r"""function uppercaser;
            string match --quiet '*nope*' $argv[1] && return 1
            string trim --chars @ $argv | string upper
        end"""
)
expect_prompt()
sendline(r"abbr uppercaser --regex '@.+@' --function uppercaser")
expect_prompt()
send(r"@abc@ ?")
expect_str(r"<ABC >")
send(r"@nope@ ?")
expect_str(r"<@nope@ >")
sendline(r"abbr --erase uppercaser")
expect_prompt()

# -f works as shorthand for --function.
sendline(r"abbr uppercaser2 --regex '@.+@' -f uppercaser")
expect_prompt()
send(r"@abc@ ?")
expect_str(r"<ABC >")
send(r"@nope@ ?")
expect_str(r"<@nope@ >")
sendline(r"abbr --erase uppercaser2")
expect_prompt()

# Verify that 'commandline' is accurate.
# Abbreviation functions cannot usefully change the command line, but they can read it.
sendline(
    r"""function check_cmdline
            set -g last_cmdline (commandline)
            set -g last_cursor (commandline --cursor)
            false
        end"""
)
expect_prompt()
sendline(r"abbr check_cmdline --regex '@.+@' --function check_cmdline")
expect_prompt()
send(r"@abc@ ?")
expect_str(r"<@abc@ >")
sendline(r"echo $last_cursor - $last_cmdline")
expect_prompt(r"6 - @abc@ ")
