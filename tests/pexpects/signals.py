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
import platform
import signal
import subprocess
import sys

if "CI" in os.environ and platform.system() in ["Darwin", "FreeBSD"]:
    sys.exit(127)

expect_prompt()

# Verify that SIGINT inside a command sub cancels it.
# Negate the pid to send to the pgroup (which should include sleep).
sendline("while true; echo (sleep 2000); end")
sleep(0.5)
os.kill(-sp.spawn.pid, signal.SIGINT)
expect_prompt()
sleep(0.2)

# SIGINT should be ignored by background processes.
sendline("sleep 1234 &")
expect_prompt()
os.kill(-sp.spawn.pid, signal.SIGINT)
sleep(0.500)
sendline("jobs")
expect_prompt("sleep 1234")
sendline("kill %1")
expect_prompt()

# Verify that the fish_postexec handler is called after SIGINT.
sendline("function postexec --on-event fish_postexec; echo fish_postexec spotted; end")
expect_prompt()
sendline("read")
expect_re(r"\r\n?.*read> .*", timeout=10)
sleep(0.1)
os.kill(sp.spawn.pid, signal.SIGINT)
expect_str("fish_postexec spotted", timeout=10)
expect_prompt()

# Ensure that we catch up.
sendline("echo 'hello' 'world'")
expect_prompt("hello world")

# Verify that the fish_kill_signal is set.
sendline(
    "functions -e postexec; function postexec --on-event fish_postexec; echo fish_kill_signal $fish_kill_signal; end"
)
expect_prompt("fish_kill_signal 0")
sendline("sleep 5")
sleep(0.300)
subprocess.call(["pkill", "-INT", "-P", str(sp.spawn.pid), "sleep"])
expect_str("fish_kill_signal 2")
expect_prompt()

sendline("sleep 5")
sleep(0.200)
subprocess.call(["pkill", "-TERM", "-P", str(sp.spawn.pid), "sleep"])
expect_str("fish_kill_signal 15")
expect_prompt()

# See that open() is only interruptible by SIGINT.
sendline("mkfifo fifoo")
expect_prompt()
sendline("cat >fifoo")
subprocess.call(["kill", "-WINCH", str(sp.spawn.pid)])
expect_re("open: ", shouldfail=True, timeout=2)
subprocess.call(["kill", "-INT", str(sp.spawn.pid)])
expect_prompt()

# Verify that sending SIGHUP to the shell, such as will happen when the tty is
# closed by the terminal, terminates the shell and the foreground command and
# any background commands run from that shell.
#
# Save the pids for later to check if they are still running.
pids = []
send("sleep 1300 & echo $last_pid\r")
pids += [expect_re("\\d+\r\n").group().strip()]
expect_prompt()
sendline("echo 'catch' 'up' 'A'")
expect_prompt("catch up A")
send("sleep 1310 & echo $last_pid\r")
pids += [expect_re("\\d+\r\n").group().strip()]
expect_prompt()
sendline("echo 'catch' 'up' 'B'")
expect_prompt("catch up B")
send("sleep 9999999\r")
sleep(0.500)  # ensure fish kicks off the above sleep before it gets HUP - see #7288
os.kill(sp.spawn.pid, signal.SIGHUP)

# Verify the spawned fish shell has exited.
sp.spawn.wait()

# Verify all child processes have been killed. We don't use `-p $pid` because
# if the shell has a bug the child processes might have been reparented to pid
# 1 rather than killed.
proc = subprocess.run(
    ["pgrep", "-l", "-f", "sleep 13"], stdout=subprocess.PIPE, stderr=subprocess.PIPE
)

remaining = []
if proc.returncode == 0:
    # If any sleeps exist, we check them against our pids,
    # to avoid false-positives (any other `sleep 13xyz` running on the system)
    print(proc.stdout)
    for line in proc.stdout.split(b"\n"):
        pid = line.split(b" ", maxsplit=1)[0].decode("utf-8")
        if pid in pids:
            remaining += [pid]

# Kill any remaining sleeps ourselves, otherwise rerunning this is pointless.
for pid in remaining:
    try:
        os.kill(int(pid), signal.SIGTERM)
    except ProcessLookupError:
        continue

if remaining:
    # We still have processes left over!
    print("Commands were still running!")
    print(remaining)
    sys.exit(1)
