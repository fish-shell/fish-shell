#!/usr/bin/env python3
from pexpect_helper import SpawnedProc
import os
import platform
import sys

# Skip on macOS on Github Actions because it's too resource-starved
# and fails this a lot.
#
# Presumably we still have users on macOS that would notice binding errors
if "CI" in os.environ and platform.system() == "Darwin":
    sys.exit(127)

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

# Clear twice (regression test for #7280).
send("\f")
expect_prompt(increment=False)
send("\f")
expect_prompt(increment=False)

# Fish should start in default-mode (i.e., emacs) bindings. The default escape
# timeout is 30ms.
#
# Because common CI systems are awful, we have to increase this:

sendline("set -g fish_escape_delay_ms 120")
expect_prompt("")

# Verify the emacs transpose word (\et) behavior using various delays,
# including none, after the escape character.

# Start by testing with no delay. This should transpose the words.
send("echo abc def")
send("\033t\r")
expect_prompt("\r\n.*def abc\r\n")  # emacs transpose words, default timeout: no delay

# Now test with a delay > 0 and < the escape timeout. This should transpose
# the words.
send("echo ghi jkl")
send("\033")
sleep(0.010)
send("t\r")
# emacs transpose words, default timeout: short delay
expect_prompt("\r\n.*jkl ghi\r\n")

# Now test with a delay > the escape timeout. The transposition should not
# occur and the "t" should become part of the text that is echoed.
send("echo mno pqr")
send("\033")
sleep(0.250)
send("t\r")
# emacs transpose words, default timeout: long delay
expect_prompt("\r\n.*mno pqrt\r\n")

# Now test that exactly the expected bind modes are defined
sendline("bind --list-modes")
expect_prompt("\r\n.*default", unmatched="Unexpected bind modes")

# Test vi key bindings.
# This should leave vi mode in the insert state.
sendline("set -g fish_key_bindings fish_vi_key_bindings")
expect_prompt()

# Go through a prompt cycle to let fish catch up, it may be slow due to ASAN
sendline("echo success: default escape timeout")
expect_prompt(
    "\r\n.*success: default escape timeout", unmatched="prime vi mode, default timeout"
)

send("echo fail: default escape timeout")
expect_str("echo fail: default escape timeout")
send("\033")

# Delay needed to allow fish to transition to vi "normal" mode. The delay is
# longer than strictly necessary to let fish catch up as it may be slow due to
# ASAN.
sleep(0.250)
send("ddi")
sendline("echo success: default escape timeout")
expect_prompt(
    "\r\n.*success: default escape timeout\r\n",
    unmatched="vi replace line, default timeout: long delay",
)

# Test replacing a single character.
send("echo TEXT")
send("\033")
# Delay needed to allow fish to transition to vi "normal" mode.
# Specifically alt+h *is* bound to __fish_man_page,
# and I have seen this think that trigger with 300ms.
#
# The next step is to rip out this test because it's much more pain than it is worth
sleep(0.400)
send("hhrAi\r")
expect_prompt(
    "\r\n.*TAXT\r\n", unmatched="vi mode replace char, default timeout: long delay"
)

# Test deleting characters with 'x'.
send("echo MORE-TEXT")
send("\033")
# Delay needed to allow fish to transition to vi "normal" mode.
sleep(0.400)
send("xxxxx\r")

# vi mode delete char, default timeout: long delay
expect_prompt(
    "\r\n.*MORE\r\n", unmatched="vi mode delete char, default timeout: long delay"
)

# Test jumping forward til before a character with t
send("echo MORE-TEXT-IS-NICE")
send("\033")
# Delay needed to allow fish to transition to vi "normal" mode.
sleep(0.250)
send("0tTD\r")

# vi mode forward-jump-till character, default timeout: long delay
expect_prompt(
    "\r\n.*MORE\r\n",
    unmatched="vi mode forward-jump-till character, default timeout: long delay",
)

# DISABLED BECAUSE IT FAILS ON GITHUB ACTIONS
# Test jumping backward til before a character with T
# send("echo MORE-TEXT-IS-NICE")
# send("\033")
# # Delay needed to allow fish to transition to vi "normal" mode.
# sleep(0.250)
# send("TSD\r")
# # vi mode backward-jump-till character, default timeout: long delay
# expect_prompt(
#     "\r\n.*MORE-TEXT-IS\r\n",
#     unmatched="vi mode backward-jump-till character, default timeout: long delay",
# )

# Test jumping backward with F and repeating
send("echo MORE-TEXT-IS-NICE")
send("\033")
# Delay needed to allow fish to transition to vi "normal" mode.
sleep(0.250)
send("F-;D\r")
# vi mode backward-jump-to character and repeat, default timeout: long delay
expect_prompt(
    "\r\n.*MORE-TEXT\r\n",
    unmatched="vi mode backward-jump-to character and repeat, default timeout: long delay",
)

# Test jumping backward with F w/reverse jump
send("echo MORE-TEXT-IS-NICE")
send("\033")
# Delay needed to allow fish to transition to vi "normal" mode.
sleep(0.250)
send("F-F-,D\r")
# vi mode backward-jump-to character, and reverse, default timeout: long delay
expect_prompt(
    "\r\n.*MORE-TEXT-IS\r\n",
    unmatched="vi mode backward-jump-to character, and reverse, default timeout: long delay",
)

# Verify that changing the escape timeout has an effect.
send("set -g fish_escape_delay_ms 100\r")
expect_prompt()

send("echo fail: lengthened escape timeout")
send("\033")
sleep(0.400)
send("ddi")
sleep(0.25)
send("echo success: lengthened escape timeout\r")
expect_prompt(
    "\r\n.*success: lengthened escape timeout\r\n",
    unmatched="vi replace line, 100ms timeout: long delay",
)

# Verify that we don't switch to vi normal mode if we don't wait long enough
# after sending escape.
send("echo fail: no normal mode")
send("\033")
sleep(0.010)
send("ddi")
send("inserted\r")
expect_prompt(
    "\r\n.*fail: no normal modediinserted\r\n",
    unmatched="vi replace line, 100ms timeout: short delay",
)

# Now set it back to speed up the tests - these don't use any escape+thing bindings!
send("set -g fish_escape_delay_ms 50\r")
expect_prompt()

# Test 't' binding that contains non-zero arity function (forward-jump) followed
# by another function (and) https://github.com/fish-shell/fish-shell/issues/2357
send("\033")
sleep(0.200)
send("ddiecho TEXT")
expect_str("echo TEXT")
send("\033")
sleep(0.200)
send("hhtTrN\r")
expect_prompt("\r\n.*TENT\r\n", unmatched="Couldn't find expected output 'TENT'")

# Test sequence key delay
send("set -g fish_sequence_key_delay_ms 200\r")
expect_prompt()
send("bind -M insert jk 'commandline -i foo'\r")
expect_prompt()
send("echo jk")
send("\r")
expect_prompt("foo")
send("echo j")
sleep(0.300)
send("k\r")
expect_prompt("jk")
send("set -e fish_sequence_key_delay_ms\r")
expect_prompt()
send("echo j")
sleep(0.300)
send("k\r")
expect_prompt("foo")


# Test '~' (togglecase-char)
# HACK: Deactivated because it keeps failing on CI
# send("\033")
# sleep(0.100)
# send("cc")
# sleep(0.50)
# send("echo some TExT\033")
# sleep(0.300)
# send("hh~~bbve~\r")
# expect_prompt("\r\n.*SOME TeXT\r\n", unmatched="Couldn't find expected output 'SOME TeXT")

# Now test that exactly the expected bind modes are defined
sendline("bind --list-modes")
expect_prompt(
    "default\r\ninsert\r\nreplace\r\nreplace_one\r\nvisual\r\n",
    unmatched="Unexpected vi bind modes",
)

# Switch back to regular (emacs mode) key bindings.
sendline("set -g fish_key_bindings fish_default_key_bindings")
expect_prompt()

# Verify the custom escape timeout set earlier is still in effect.
sendline("echo fish_escape_delay_ms=$fish_escape_delay_ms")
expect_prompt(
    "\r\n.*fish_escape_delay_ms=50\r\n",
    unmatched="default-mode custom timeout not set correctly",
)

sendline("set -g fish_escape_delay_ms 200")
expect_prompt()

# Verify the emacs transpose word (\et) behavior using various delays,
# including none, after the escape character.

# Start by testing with no delay. This should transpose the words.
send("echo abc def")
send("\033")
send("t\r")
expect_prompt(
    "\r\n.*def abc\r\n", unmatched="emacs transpose words fail, 200ms timeout: no delay"
)

# Verify special characters, such as \cV, are not intercepted by the kernel
# tty driver. Rather, they can be bound and handled by fish.
sendline("bind ctrl-v 'echo ctrl-v seen'")
expect_prompt()
send("\026\r")
expect_prompt("ctrl-v seen", unmatched="ctrl-v not seen")

send("bind ctrl-o 'echo ctrl-o seen'\r")
expect_prompt()
send("\017\r")
expect_prompt("ctrl-o seen", unmatched="ctrl-o not seen")

# \x17 is ctrl-w.
send("echo git@github.com:fish-shell/fish-shell")
send("\x17\x17\r")
expect_prompt("git@github.com:", unmatched="ctrl-w does not stop at :")

send("echo git@github.com:fish-shell/fish-shell")
send("\x17\x17\x17\r")
expect_prompt("git@", unmatched="ctrl-w does not stop at @")

sendline("abbr --add foo 'echo foonanana'")
expect_prompt()
sendline("bind ' ' expand-abbr or self-insert")
expect_prompt()
send("foo ")
expect_str("echo foonanana")
send(" banana\r")
expect_str(" banana\r")
expect_prompt("foonanana banana")

# Ensure that nul can be bound properly (#3189).
send("bind -k nul 'echo nul seen'\r")
expect_prompt()
send("\0" * 3)
# We need to sleep briefly before emitting a newline, because when we execute the
# key bindings above we place the tty in external-proc mode (see #7483) and restoring
# the mode to shell-mode races with the newline emitted below (i.e. sometimes it may
# be echoed).
sleep(0.1)
send("\r")
expect_prompt("nul seen\r\n.*nul seen\r\n.*nul seen", unmatched="nul not seen")

# Test self-insert-notfirst. (#6603)
# Here the leading 'q's should be stripped, but the trailing ones not.
sendline("bind q self-insert-notfirst")
expect_prompt()
sendline("qqqecho qqq")
expect_prompt("qqq", unmatched="Leading qs not stripped")

# Test bigword with single-character words.
sendline("bind ctrl-g kill-bigword")
expect_prompt()
send("a b c d\x01")  # ctrl-a, move back to the beginning of the line
send("\x07")  # ctrl-g, kill bigword
sendline("echo")
expect_prompt("\n.*b c d")

# Test that overriding the escape binding works
# and does not inhibit other escape sequences (up-arrow in this case).
sendline("bind escape 'echo foo'")
expect_prompt()
send("\x1b")
expect_str("foo")
send("\x1b[A")
expect_str("bind escape 'echo foo'")
sendline("")
expect_prompt()

send("    a b c d\x01")  # ctrl-a, move back to the beginning of the line
send("\x07")  # ctrl-g, kill bigword
sendline("echo")
expect_prompt("\n.*b c d")

# Check that ctrl-z can be bound
sendline('bind ctrl-z "echo bound ctrl-z"')
expect_prompt()
send("\x1A")
expect_str("bound ctrl-z")

send('echo foobar')
send('\x02\x02\x02') # ctrl-b, backward-char
sendline('\x1bu') # alt+u, upcase word
expect_prompt("fooBAR")

sendline("""
    bind ctrl-g "
        commandline --insert 'echo foo ar'
        commandline -f backward-word
        commandline --insert b
        commandline -f backward-char
        commandline -f backward-char
        commandline -f delete-char
    "
""".strip())
send('\x07') # ctrl-g
send('\r')
expect_prompt("foobar")

# This should do nothing instead of crash
sendline("commandline -f backward-jump")
expect_prompt()
sendline("commandline -f self-insert")
expect_prompt()
sendline("commandline -f and")
expect_prompt()

# Check that the builtin version of `exit` works
# (for obvious reasons this MUST BE LAST)
sendline("function myexit; echo exit; exit; end; bind ctrl-z myexit")
expect_prompt()
send("\x1A")
expect_str("exit")

for t in range(0, 50):
    if not sp.spawn.isalive():
        break
    # This is cheesy, but on CI with thread-sanitizer this can be slow enough that the process is still running, so we sleep for a bit.
    sleep(0.1)
else:
    print("Fish did not exit via binding!")
    sys.exit(1)
