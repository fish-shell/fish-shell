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

send(r"alpha;?")
expect_str(r"<beta;>")

# Abbreviation expansions may have multiple words.
sendline(r"abbr --add emacsnw emacs -nw -l")
expect_prompt()
send(r"emacsnw ?")
expect_str(r"<emacs -nw -l >")

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
send(r"QA123Z ?")  # fails as the regex must match the entire string
expect_str(r"<QA123Z >")
send(r"A0000000000000000000009Z ?")
expect_str(r"<beta3 >")

# Support functions. Here anything in between @@ is uppercased, except 'nope'.
sendline(
    r"""function uppercaser
            string match --quiet '*nope*' $argv[1] && return 1
            string trim --chars @ $argv | string upper
        end
    """.strip()
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

# Abbreviations which cause the command line to become incomplete or invalid
# are visibly expanded.
sendline(r"abbr openparen --position anywhere '('")
expect_prompt()
sendline(r"abbr closeparen --position anywhere ')'")
expect_prompt()
sendline(r"echo openparen")
expect_str(r"echo (")
send(r"?")
expect_str(r"<echo (>")
sendline(r"echo closeparen")
expect_str(r"echo )")
send(r"?")
expect_str(r"<echo )>")
sendline(r"echo openparen seq 5 closeparen")  # expands on enter
expect_prompt(r"1 2 3 4 5")
sendline(r"echo openparen seq 5 closeparen ")  # expands on space
expect_prompt(r"1 2 3 4 5")


# Verify that 'commandline' is accurate.
# Abbreviation functions cannot usefully change the command line, but they can read it.
sendline(
    r"""function check_cmdline
            set -g last_cmdline (commandline)
            set -g last_cursor (commandline --cursor)
            false
        end
    """.strip()
)
expect_prompt()
sendline(r"abbr check_cmdline --regex '@.+@' --function check_cmdline")
expect_prompt()
send(r"@abc@ ?")
expect_str(r"<@abc@ >")
sendline(r"echo $last_cursor : $last_cmdline")
expect_prompt(r"6 : @abc@ ")


# Test cursor positioning.
sendline(r"""abbr --erase (abbr --list) """)
expect_prompt()
sendline(r"""abbr LLL --position anywhere --set-cursor 'abc%ghi'""")
expect_prompt()
send(r"""echo LLL def?""")
expect_str(r"<echo abcdefghi >")

sendline(r"""abbr --erase (abbr --list) """)
expect_prompt()
sendline(r"""abbr LLL --position anywhere --set-cursor=!HERE! '!HERE! | less'""")
expect_prompt()
send(r"""echo LLL derp?""")
expect_str(r"<echo derp | less >")

sendline(r"""abbr foo --command echo bar""")
expect_prompt()
sendline(r"""printf '%s\n' foo """)
expect_prompt("foo")
sendline(r"""echo foo """)
expect_prompt("bar")
sendline(r"""true; and echo foo """)
expect_prompt("bar")
sendline(r"""true; and builtin echo foo """)
expect_prompt("bar")
sendline(r"""abbr fruit --command={git,hg,svn} banana""")
expect_prompt()
sendline(r"""function git; echo git $argv; end; function hg; echo hg $argv; end; function svn; echo svn $argv; end""")
expect_prompt()
sendline(r"""git fruit""")
expect_prompt("git banana")
sendline(r"""abbr""")
expect_prompt("abbr -a --position anywhere --command git --command hg --command svn -- fruit banana")
sendline(r"""function banana; echo I am a banana; end""")
expect_prompt()
sendline(r"""abbr fruit --command={git,hg,svn,} banana""")
expect_prompt()
sendline(r"""fruit foo""")
expect_prompt("I am a banana")

