#!/usr/bin/env python3
from pexpect_helper import SpawnedProc
import sys
import signal
import os

sp = SpawnedProc()
send, sendline, sleep, expect_prompt, expect_re = (
    sp.send,
    sp.sendline,
    sp.sleep,
    sp.expect_prompt,
    sp.expect_re,
)


# Helper to print an error and exit.
def error_and_exit(text):
    keys = sp.colors()
    print("{RED}Test failed: {NORMAL}{TEXT}".format(TEXT=text, **keys))
    sys.exit(1)


fish_pid = sp.spawn.pid

# Launch fish_test_helper.
expect_prompt()
exe_path = os.environ.get("fish_test_helper")
if not exe_path:
    sys.exit(127)
if not os.path.exists(exe_path):
    print(f"{exe_path} does not exist")
    sys.exit(1)

sp.sendline(exe_path + " nohup_wait")

# We expect it to transfer tty ownership to fish_test_helper.
sleep(1)
tty_owner = os.tcgetpgrp(sp.spawn.child_fd)
if fish_pid == tty_owner:
    os.kill(fish_pid, signal.SIGKILL)
    error_and_exit(
        "Test failed: outer fish %d did not transfer tty owner to fish_test_helper"
        % (fish_pid)
    )


# Now we are going to tell fish to exit.
# It must not hang. But it might hang when trying to restore the tty.
os.kill(fish_pid, signal.SIGTERM)

# Loop a bit until the process exits (correct) or stops (incorrect).
# When it exits it should be due to the SIGTERM that we sent it.
for i in range(50):
    pid, status = os.waitpid(fish_pid, os.WUNTRACED | os.WNOHANG)
    if pid == 0:
        # No process ready yet, loop again.
        sleep(0.1)
    elif pid != fish_pid:
        # Some other process exited (??)
        os.kill(fish_pid, signal.SIGKILL)
        error_and_exit(
            "unexpected pid returned from waitpid. Got %d, expected %d"
            % (pid, fish_pid)
        )
    elif os.WIFSTOPPED(status):
        # Our pid stopped instead of exiting.
        # Probably it stopped because of its tcsetpgrp call during exit.
        # Kill it and report a failure.
        os.kill(fish_pid, signal.SIGKILL)
        error_and_exit("pid %d stopped instead of exiting on SIGTERM" % pid)
    elif not os.WIFSIGNALED(status):
        error_and_exit("pid %d did not signal-exit" % pid)
    elif os.WTERMSIG(status) != signal.SIGTERM:
        error_and_exit(
            "pid %d signal-exited with %d instead of %d (SIGTERM)"
            % (os.WTERMSIG(status), signal.SIGTERM)
        )
    else:
        # Success!
        sys.exit(0)
else:
    # Our loop completed without the process being returned.
    os.kill(fish_pid, signal.SIGKILL)
    error_and_exit("fish with pid %d hung after SIGTERM" % fish_pid)

# Should never get here.
os.kill(fish_pid, signal.SIGKILL)
error_and_exit("unknown, should be unreachable")
