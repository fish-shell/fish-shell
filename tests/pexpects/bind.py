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

# Clear twice (regression test for #7280).
send("\f")
expect_prompt(increment=False)
send("\f")
expect_prompt(increment=False)

# Fish should start in default-mode (i.e., emacs) bindings. The default escape
# timeout is 30ms.
#
# Because common CI systems are awful, we have to increase this:

sendline("set -g fish_escape_delay_ms 80")
expect_prompt("")

# Verify the emacs transpose word (\et) behavior using various delays,
# including none, after the escape character.

# Start by testing with no delay. This should transpose the words.
send("echo abc def")
send("\033t\r")
expect_prompt("\r\ndef abc\r\n")  # emacs transpose words, default timeout: no delay

# Now test with a delay > 0 and < the escape timeout. This should transpose
# the words.
send("echo ghi jkl")
send("\033")
sleep(0.010)
send("t\r")
# emacs transpose words, default timeout: short delay
expect_prompt("\r\njkl ghi\r\n")

# Now test with a delay > the escape timeout. The transposition should not
# occur and the "t" should become part of the text that is echoed.
send("echo mno pqr")
send("\033")
sleep(0.220)
send("t\r")
# emacs transpose words, default timeout: long delay
expect_prompt("\r\nmno pqrt\r\n")

# Now test that exactly the expected bind modes are defined
sendline("bind --list-modes")
expect_prompt("\r\ndefault\r\npaste", unmatched="Unexpected bind modes")

# Test vi key bindings.
# This should leave vi mode in the insert state.
sendline("set -g fish_key_bindings fish_vi_key_bindings")
expect_prompt()

# Go through a prompt cycle to let fish catch up, it may be slow due to ASAN
sendline("echo success: default escape timeout")
expect_prompt(
    "\r\nsuccess: default escape timeout", unmatched="prime vi mode, default timeout"
)

send("echo fail: default escape timeout")
send("\033")

# Delay needed to allow fish to transition to vi "normal" mode. The delay is
# longer than strictly necessary to let fish catch up as it may be slow due to
# ASAN.
sleep(0.250)
send("ddi")
sendline("echo success: default escape timeout")
expect_prompt(
    "\r\nsuccess: default escape timeout\r\n",
    unmatched="vi replace line, default timeout: long delay",
)

# Test replacing a single character.
send("echo TEXT")
send("\033")
# Delay needed to allow fish to transition to vi "normal" mode.
sleep(0.250)
send("hhrAi\r")
expect_prompt(
    "\r\nTAXT\r\n", unmatched="vi mode replace char, default timeout: long delay"
)

# Test deleting characters with 'x'.
send("echo MORE-TEXT")
send("\033")
# Delay needed to allow fish to transition to vi "normal" mode.
sleep(0.250)
send("xxxxx\r")

# vi mode delete char, default timeout: long delay
expect_prompt(
    "\r\nMORE\r\n", unmatched="vi mode delete char, default timeout: long delay"
)

# Test jumping forward til before a character with t
send("echo MORE-TEXT-IS-NICE")
send("\033")
# Delay needed to allow fish to transition to vi "normal" mode.
sleep(0.250)
send("0tTD\r")

# vi mode forward-jump-till character, default timeout: long delay
expect_prompt(
    "\r\nMORE\r\n",
    unmatched="vi mode forward-jump-till character, default timeout: long delay",
)

# Test jumping backward til before a character with T
send("echo MORE-TEXT-IS-NICE")
send("\033")
# Delay needed to allow fish to transition to vi "normal" mode.
sleep(0.250)
send("TSD\r")
# vi mode backward-jump-till character, default timeout: long delay
expect_prompt(
    "\r\nMORE-TEXT-IS\r\n",
    unmatched="vi mode backward-jump-till character, default timeout: long delay",
)

# Test jumping backward with F and repeating
send("echo MORE-TEXT-IS-NICE")
send("\033")
# Delay needed to allow fish to transition to vi "normal" mode.
sleep(0.250)
send("F-;D\r")
# vi mode backward-jump-to character and repeat, default timeout: long delay
expect_prompt(
    "\r\nMORE-TEXT\r\n",
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
    "\r\nMORE-TEXT-IS\r\n",
    unmatched="vi mode backward-jump-to character, and reverse, default timeout: long delay",
)

# Verify that changing the escape timeout has an effect.
send("set -g fish_escape_delay_ms 200\r")
expect_prompt()

send("echo fail: lengthened escape timeout")
send("\033")
sleep(0.350)
send("ddi")
send("echo success: lengthened escape timeout\r")
expect_prompt(
    "\r\nsuccess: lengthened escape timeout\r\n",
    unmatched="vi replace line, 200ms timeout: long delay",
)

# Verify that we don't switch to vi normal mode if we don't wait long enough
# after sending escape.
send("echo fail: no normal mode")
send("\033")
sleep(0.010)
send("ddi")
send("inserted\r")
expect_prompt(
    "\r\nfail: no normal modediinserted\r\n",
    unmatched="vi replace line, 200ms timeout: short delay",
)

# Now set it back to speed up the tests - these don't use any escape+thing bindings!
send("set -g fish_escape_delay_ms 50\r")
expect_prompt()

# Test 't' binding that contains non-zero arity function (forward-jump) followed
# by another function (and) https://github.com/fish-shell/fish-shell/issues/2357
send("\033")
sleep(0.200)
send("ddiecho TEXT\033")
sleep(0.200)
send("hhtTrN\r")
expect_prompt("\r\nTENT\r\n", unmatched="Couldn't find expected output 'TENT'")

# Test '~' (togglecase-char)
send("\033")
sleep(0.200)
send("cc")
sleep(0.01)
send("echo some TExT\033")
sleep(0.200)
send("hh~~bbve~\r")
expect_prompt("\r\nSOME TeXT\r\n", unmatched="Couldn't find expected output 'SOME TeXT")

# Now test that exactly the expected bind modes are defined
sendline("bind --list-modes")
expect_prompt(
    "\r\ndefault\r\ninsert\r\npaste\r\nreplace\r\nreplace_one\r\nvisual\r\n",
    unmatched="Unexpected vi bind modes",
)

# Switch back to regular (emacs mode) key bindings.
sendline("set -g fish_key_bindings fish_default_key_bindings")
expect_prompt()

# Verify the custom escape timeout set earlier is still in effect.
sendline("echo fish_escape_delay_ms=$fish_escape_delay_ms")
expect_prompt(
    "\r\nfish_escape_delay_ms=50\r\n",
    unmatched="default-mode custom timeout not set correctly",
)

# Set it to 100ms.
sendline("set -g fish_escape_delay_ms 100")
expect_prompt()

# Verify the emacs transpose word (\et) behavior using various delays,
# including none, after the escape character.

# Start by testing with no delay. This should transpose the words.
send("echo abc def")
send("\033")
send("t\r")
# emacs transpose words, 100ms timeout: no delay
expect_prompt(
    "\r\ndef abc\r\n", unmatched="emacs transpose words fail, 100ms timeout: no delay"
)

# Same test as above but with a slight delay less than the escape timeout.
send("echo ghi jkl")
send("\033")
sleep(0.020)
send("t\r")
# emacs transpose words, 100ms timeout: short delay
expect_prompt(
    "\r\njkl ghi\r\n",
    unmatched="emacs transpose words fail, 100ms timeout: short delay",
)

# Now test with a delay > the escape timeout. The transposition should not
# occur and the "t" should become part of the text that is echoed.
send("echo mno pqr")
send("\033")
sleep(0.250)
send("t\r")
# emacs transpose words, 100ms timeout: long delay
expect_prompt(
    "\r\nmno pqrt\r\n",
    unmatched="emacs transpose words fail, 100ms timeout: long delay",
)

# Verify special characters, such as \cV, are not intercepted by the kernel
# tty driver. Rather, they can be bound and handled by fish.
sendline("bind \\cV 'echo ctrl-v seen'")
expect_prompt()
send("\026\r")
expect_prompt("ctrl-v seen", unmatched="ctrl-v not seen")

send("bind \\cO 'echo ctrl-o seen'\r")
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

# Ensure that nul can be bound properly (#3189).
send("bind -k nul 'echo nul seen'\r")
expect_prompt()
send("\0" * 3)
send("\r")
expect_prompt("nul seen\r\nnul seen\r\nnul seen", unmatched="nul not seen")

# Test self-insert-notfirst. (#6603)
# Here the leading 'q's should be stripped, but the trailing ones not.
sendline("bind q self-insert-notfirst")
expect_prompt()
sendline("qqqecho qqq")
expect_prompt("qqq", unmatched="Leading qs not stripped")

# Test bigword with single-character words.
sendline("bind \cg kill-bigword")
expect_prompt()
send("a b c d\x01")  # ctrl-a, move back to the beginning of the line
send("\x07")  # ctrl-g, kill bigword
sendline("echo")
expect_prompt("\nb c d")

send("    a b c d\x01")  # ctrl-a, move back to the beginning of the line
send("\x07")  # ctrl-g, kill bigword
sendline("echo")
expect_prompt("\nb c d")

# Check that ctrl-z can be bound
sendline('bind \cz "echo bound ctrl-z"')
expect_prompt()
send('\x1A')
expect_str("bound ctrl-z")
