#!/usr/bin/env python3
import os
import platform
import signal
import sys
from pexpect_helper import SpawnedProc

if "CI" in os.environ and platform.system() in ["Darwin", "FreeBSD"]:
    sys.exit(127)

sp = SpawnedProc()
send, sendline, sleep, expect_prompt, expect_str, expect_re = (
    sp.send,
    sp.sendline,
    sp.sleep,
    sp.expect_prompt,
    sp.expect_str,
    sp.expect_re,
)

# Ensure that signals don't tear escape sequences. See #8628.
expect_prompt()

# Allow for long delays before matching escape.
sendline(r"set -g fish_escape_delay_ms 2000")
expect_prompt()

# Set up a handler for SIGUSR1.
sendline(r"set -g sigusr1_count 0")
expect_prompt()
sendline(r"set -g wacky_count 0")
expect_prompt()

sendline(
    r"""
    function usr1_handler --on-signal SIGUSR1;
        set sigusr1_count (math $sigusr1_count + 1);
        echo Got SIGUSR1 $sigusr1_count;
        commandline -f repaint;
    end
""".strip().replace(
        "\n", os.linesep
    )
)
expect_prompt()

# Set up a wacky binding with an escape.
sendline(
    r"""
    function wacky_handler
        set wacky_count (math $wacky_count + 1);
        echo Wacky Handler $wacky_count
    end
""".strip().replace(
        "\n", os.linesep
    )
)
expect_prompt()

sendline(r"bind a,b,c,\e,d,e,f wacky_handler")
expect_prompt()

sendline("echo 'Catch' 'up'")
expect_prompt("Catch up")

# We can respond to SIGUSR1.
sleep(1)
os.kill(sp.spawn.pid, signal.SIGUSR1)
expect_str(r"Got SIGUSR1 1")
sendline(r"")
expect_prompt()

# Our wacky binding works.
send("abc\x1bdef")
expect_str(r"Wacky Handler 1")
sendline(r"")
expect_prompt()

# Now we interleave the sequence with SIGUSR1.
# What we expect to happen is that the signal is "shuffled to the front" ahead of the key events.
send("abc")
sleep(0.05)
os.kill(sp.spawn.pid, signal.SIGUSR1)
sleep(0.05)
send("\x1bdef")
expect_str(r"Got SIGUSR1 2")
expect_str(r"Wacky Handler 2")
sendline(r"")
expect_prompt()

# As before but it comes after the ESC.
# The signal will arrive while we are waiting in readch_timed().
send("abc\x1b")
sleep(0.05)
os.kill(sp.spawn.pid, signal.SIGUSR1)
sleep(0.05)
send("def")
expect_str(r"Got SIGUSR1 3")
expect_str(r"Wacky Handler 3")
sendline(r"")
expect_prompt()
