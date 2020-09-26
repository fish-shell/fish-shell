#!/usr/bin/env python3
from pexpect_helper import SpawnedProc

sp = SpawnedProc(timeout=10)
send, sendline, sleep, expect_prompt, expect_re, expect_str = (
    sp.send,
    sp.sendline,
    sp.sleep,
    sp.expect_prompt,
    sp.expect_re,
    sp.expect_str,
)

from time import sleep
import os
import signal
import subprocess
import sys

expect_prompt()

sendline("sleep 10 &")
expect_prompt()

send("\x03")
sleep(0.010)
sendline("jobs")
expect_prompt("sleep.10")
sendline("kill %1")
expect_prompt()

# Verify that the fish_postexec handler is called after SIGINT.
sendline("function postexec --on-event fish_postexec; echo fish_postexec spotted; end")
expect_prompt()
sendline("read")
expect_re("\r\n?read> $")
sleep(0.100)
os.kill(sp.spawn.pid, signal.SIGINT)
expect_str("fish_postexec spotted")
expect_prompt()

# Verify that the fish_kill_signal is set.
sendline(
    "functions -e postexec; function postexec --on-event fish_postexec; echo fish_kill_signal $fish_kill_signal; end"
)
expect_prompt()
sendline("sleep 5")
sleep(0.100)
subprocess.call(["pkill", "-INT", "sleep", "-P", str(sp.spawn.pid)])
expect_str("fish_kill_signal 2")
expect_prompt()

sendline("sleep 5")
sleep(0.100)
subprocess.call(["pkill", "-TERM", "sleep", "-P", str(sp.spawn.pid)])
expect_str("fish_kill_signal 15")
expect_prompt()

# Verify that sending SIGHUP to the shell, such as will happen when the tty is
# closed by the terminal, terminates the shell and the foreground command and
# any background commands run from that shell.
send("sleep 130 &\r")
expect_prompt()
send("sleep 131 &\r")
expect_prompt()
send("sleep 9999999\r")
sleep(0.100)  # ensure fish kicks off the above sleep before it gets HUP - see #7288
os.kill(sp.spawn.pid, signal.SIGHUP)

# Verify the spawned fish shell has exited.
sp.spawn.wait()

# Verify all child processes have been killed. We don't use `-p $pid` because
# if the shell has a bug the child processes might have been reparented to pid
# 1 rather than killed.
proc = subprocess.run(
    ["pgrep", "-l", "-f", "sleep 13"], stdout=subprocess.PIPE, stderr=subprocess.PIPE
)
if proc.returncode == 0:
    print("Commands still running")
    print(proc.stdout)
    sys.exit(1)
