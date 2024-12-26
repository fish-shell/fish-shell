#!/usr/bin/env python3
from pexpect_helper import SpawnedProc
import platform
import subprocess
import os

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

# Hack: NetBSD's sleep likes quitting when waking up
# (but also does so under /bin/sh)
testproc = "sleep 500" if platform.system() != "NetBSD" else "cat"
sendline(testproc)
sendline("set -l foo 'bar'1; echo $foo") # ignored because sleep is in fg
sleep(1.2)

# ctrl-z - send job to background
send("\x1A")
sleep(1.2)
expect_prompt()
sendline("set -l foo 'bar'2; echo $foo")
expect_prompt("bar2")

sendline("fg; set -l foo 'bar'3; echo $foo")
expect_str("Send job 1 (" + testproc + ") to foreground")
sleep(0.2)
# Beware: Mac pkill requires that the -P argument come before the process name,
# else the -P argument is ignored.
subprocess.call(["pkill", "-INT", "-P", str(sp.spawn.pid), "sleep"])
sleep(0.2)
# Now fish reads the buffered input, since sleep never did!
# Note fish doesn't report if a proc is killed via SIGINT.
expect_prompt("bar3")

sendline("set -l foo 'bar'4; echo $foo")
expect_prompt("bar4")

# Regression test for #7483.
# Ensure we can background a job after a different backgrounded job completes.
sendline("sleep 1")
sleep(0.1)

# ctrl-z - send job to background
send("\x1A")
sleep(0.2)
expect_prompt("has stopped")

# Bring back to fg.
sendline("fg")
sleep(1.0)

# Now sleep is done, right?
expect_prompt()
sendline("jobs")
expect_prompt("jobs: There are no jobs")

# Ensure we can do it again.
sendline("sleep 5")
sleep(0.2)
send("\x1A")
sleep(0.1)
expect_prompt("has stopped")
sendline("fg")
sleep(0.1)  # allow tty to transfer
send("\x03")  # control-c to cancel it

expect_prompt()
sendline("jobs")
expect_prompt("jobs: There are no jobs")

if not os.environ.get("fish_test_helper", ""):
    import sys
    sys.exit(127)

# Regression test for #2214: foregrounding from a key binding works!
sendline(r"bind ctrl-r 'fg >/dev/null 2>/dev/null'")
expect_prompt()
sendline("$fish_test_helper print_stop_cont")
sleep(0.2)

send("\x1A")  # ctrl-z
expect_prompt("SIGTSTP")
sleep(0.1)
send("\x12")  # ctrl-r, placing fth in foreground
expect_str("SIGCONT")

# Do it again.
send("\x1A")
expect_str("SIGTSTP")
sleep(0.1)
send("\x12")
expect_str("SIGCONT")

# End fth by sending it anything.
send("\x12")
sendline("derp")
expect_prompt()
