#!/usr/bin/env python3
from pexpect_helper import SpawnedProc

sp = SpawnedProc()
sp.expect_prompt()

# Fish should start in default-mode (i.e., emacs) bindings. The default escape
# timeout is 30ms.

# Verify the emacs transpose word (\et) behavior using various delays,
# including none, after the escape character.

# Start by testing with no delay. This should transpose the words.
sp.send("echo abc def")
sp.send("\033t\r")
sp.expect_prompt("\r\ndef abc\r\n")  # emacs transpose words, default timeout: no delay

# Now test with a delay > 0 and < the escape timeout. This should transpose
# the words.
sp.send("echo ghi jkl")
sp.send("\033")
sp.sleep(0.010)
sp.send("t\r")
# emacs transpose words, default timeout: short delay
sp.expect_prompt("\r\njkl ghi\r\n")

# Now test with a delay > the escape timeout. The transposition should not
# occur and the "t" should become part of the text that is echoed.
sp.send("echo mno pqr")
sp.send("\033")
sp.sleep(0.200)
sp.send("t\r")
# emacs transpose words, default timeout: long delay
sp.expect_prompt("\r\nmno pqrt\r\n")

# Now test that exactly the expected bind modes are defined
sp.sendline("bind --list-modes")
sp.expect_prompt("\r\ndefault\r\npaste", unmatched="Unexpected bind modes")

# Test vi key bindings.
# This should leave vi mode in the insert state.
sp.sendline("set -g fish_key_bindings fish_vi_key_bindings")
sp.expect_prompt()

# Go through a prompt cycle to let fish catch up, it may be slow due to ASAN
sp.sendline("echo success: default escape timeout")
sp.expect_prompt(
    "\r\nsuccess: default escape timeout", unmatched="prime vi mode, default timeout"
)

sp.send("echo fail: default escape timeout")
sp.send("\033")

# Delay needed to allow fish to transition to vi "normal" mode. The delay is
# longer than strictly necessary to let fish catch up as it may be slow due to
# ASAN.
sp.sleep(0.150)
sp.send("ddi")
sp.sendline("echo success: default escape timeout")
sp.expect_prompt(
    "\r\nsuccess: default escape timeout\r\n",
    unmatched="vi replace line, default timeout: long delay",
)

# Test replacing a single character.
sp.send("echo TEXT")
sp.send("\033")
# Delay needed to allow fish to transition to vi "normal" mode.
sp.sleep(0.150)
sp.send("hhrAi\r")
sp.expect_prompt(
    "\r\nTAXT\r\n", unmatched="vi mode replace char, default timeout: long delay"
)

# Test deleting characters with 'x'.
sp.send("echo MORE-TEXT")
sp.send("\033")
# Delay needed to allow fish to transition to vi "normal" mode.
sp.sleep(0.250)
sp.send("xxxxx\r")

# vi mode delete char, default timeout: long delay
sp.expect_prompt(
    "\r\nMORE\r\n", unmatched="vi mode delete char, default timeout: long delay"
)

# Test jumping forward til before a character with t
sp.send("echo MORE-TEXT-IS-NICE")
sp.send("\033")
# Delay needed to allow fish to transition to vi "normal" mode.
sp.sleep(0.250)
sp.send("0tTD\r")

# vi mode forward-jump-till character, default timeout: long delay
sp.expect_prompt(
    "\r\nMORE\r\n",
    unmatched="vi mode forward-jump-till character, default timeout: long delay",
)

# Test jumping backward til before a character with T
sp.send("echo MORE-TEXT-IS-NICE")
sp.send("\033")
# Delay needed to allow fish to transition to vi "normal" mode.
sp.sleep(0.250)
sp.send("TSD\r")
# vi mode backward-jump-till character, default timeout: long delay
sp.expect_prompt(
    "\r\nMORE-TEXT-IS\r\n",
    unmatched="vi mode backward-jump-till character, default timeout: long delay",
)

# Test jumping backward with F and repeating
sp.send("echo MORE-TEXT-IS-NICE")
sp.send("\033")
# Delay needed to allow fish to transition to vi "normal" mode.
sp.sleep(0.250)
sp.send("F-;D\r")
# vi mode backward-jump-to character and repeat, default timeout: long delay
sp.expect_prompt(
    "\r\nMORE-TEXT\r\n",
    unmatched="vi mode backward-jump-to character and repeat, default timeout: long delay",
)

# Test jumping backward with F w/reverse jump
sp.send("echo MORE-TEXT-IS-NICE")
sp.send("\033")
# Delay needed to allow fish to transition to vi "normal" mode.
sp.sleep(0.250)
sp.send("F-F-,D\r")
# vi mode backward-jump-to character, and reverse, default timeout: long delay
sp.expect_prompt(
    "\r\nMORE-TEXT-IS\r\n",
    unmatched="vi mode backward-jump-to character, and reverse, default timeout: long delay",
)

# Verify that changing the escape timeout has an effect.
sp.send("set -g fish_escape_delay_ms 200\r")
sp.expect_prompt()

sp.send("echo fail: lengthened escape timeout")
sp.send("\033")
sp.sleep(0.350)
sp.send("ddi")
sp.send("echo success: lengthened escape timeout\r")
# vi replace line, 200ms timeout: long delay
sp.expect_prompt(
    "\r\nsuccess: lengthened escape timeout\r\n",
    unmatched="vi replace line, 200ms timeout: long delay",
)

# Verify that we don't switch to vi normal mode if we don't wait long enough
# after sending escape.
sp.send("echo fail: no normal mode")
sp.send("\033")
sp.sleep(0.050)
sp.send("ddi")
sp.send("inserted\r")
# vi replace line, 200ms timeout: short delay
sp.expect_prompt(
    "\r\nfail: no normal modediinserted\r\n",
    unmatched="vi replace line, 200ms timeout: short delay",
)

# Test 't' binding that contains non-zero arity function (forward-jump) followed
# by another function (and) https://github.com/fish-shell/fish-shell/issues/2357
sp.send("\033")
sp.sleep(0.300)
sp.send("ddiecho TEXT\033")
sp.sleep(0.300)
sp.send("hhtTrN\r")
sp.expect_prompt("\r\nTENT\r\n", unmatched="Couldn't find expected output 'TENT'")

# Now test that exactly the expected bind modes are defined
sp.sendline("bind --list-modes")
sp.expect_prompt(
    "\r\ndefault\r\ninsert\r\npaste\r\nreplace\r\nreplace_one\r\nvisual\r\n",
    unmatched="Unexpected vi bind modes",
)

# Switch back to regular (emacs mode) key bindings.
sp.sendline("set -g fish_key_bindings fish_default_key_bindings")
sp.expect_prompt()

# Verify the custom escape timeout of 200ms set earlier is still in effect.
sp.sendline("echo fish_escape_delay_ms=$fish_escape_delay_ms")
sp.expect_prompt(
    "\r\nfish_escape_delay_ms=200\r\n",
    unmatched="default-mode custom timeout not set correctly",
)

# Set it to 100ms.
sp.sendline("set -g fish_escape_delay_ms 100")
sp.expect_prompt()

# Verify the emacs transpose word (\et) behavior using various delays,
# including none, after the escape character.

# Start by testing with no delay. This should transpose the words.
sp.send("echo abc def")
sp.send("\033")
sp.send("t\r")
# emacs transpose words, 100ms timeout: no delay
sp.expect_prompt(
    "\r\ndef abc\r\n", unmatched="emacs transpose words fail, 100ms timeout: no delay"
)

# Same test as above but with a slight delay less than the escape timeout.
sp.send("echo ghi jkl")
sp.send("\033")
sp.sleep(0.080)
sp.send("t\r")
# emacs transpose words, 100ms timeout: short delay
sp.expect_prompt(
    "\r\njkl ghi\r\n",
    unmatched="emacs transpose words fail, 100ms timeout: short delay",
)

# Now test with a delay > the escape timeout. The transposition should not
# occur and the "t" should become part of the text that is echoed.
sp.send("echo mno pqr")
sp.send("\033")
sp.sleep(0.250)
sp.send("t\r")
# emacs transpose words, 100ms timeout: long delay
sp.expect_prompt(
    "\r\nmno pqrt\r\n",
    unmatched="emacs transpose words fail, 100ms timeout: long delay",
)

# Verify special characters, such as \cV, are not intercepted by the kernel
# tty driver. Rather, they can be bound and handled by fish.
sp.sendline("bind \\cV 'echo ctrl-v seen'")
sp.expect_prompt()
sp.send("\026\r")
sp.expect_prompt("ctrl-v seen", unmatched="ctrl-v not seen")

sp.send("bind \\cO 'echo ctrl-o seen'\r")
sp.expect_prompt()
sp.send("\017\r")
sp.expect_prompt("ctrl-o seen", unmatched="ctrl-o not seen")

# \x17 is ctrl-w.
sp.send("echo git@github.com:fish-shell/fish-shell")
sp.send("\x17\x17\r")
sp.expect_prompt("git@github.com:", unmatched="ctrl-w does not stop at :")

sp.send("echo git@github.com:fish-shell/fish-shell")
sp.send("\x17\x17\x17\r")
sp.expect_prompt("git@", unmatched="ctrl-w does not stop at @")

# Ensure that nul can be bound properly (#3189).
sp.send("bind -k nul 'echo nul seen'\r")
sp.expect_prompt
sp.send("\0" * 3)
sp.send("\r")
sp.expect_prompt("nul seen\r\nnul seen\r\nnul seen", unmatched="nul not seen")

# Test self-insert-notfirst. (#6603)
# Here the leading 'q's should be stripped, but the trailing ones not.
sp.sendline("bind q self-insert-notfirst")
sp.expect_prompt()
sp.sendline("qqqecho qqq")
sp.expect_prompt("qqq", unmatched="Leading qs not stripped")
