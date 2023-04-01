#!/usr/bin/env python3
from pexpect_helper import SpawnedProc
import os
import signal
import tempfile
from time import sleep

# Ensure that on-exit handlers run even if we get SIGHUP.
with tempfile.NamedTemporaryFile(mode="r", encoding="utf8") as tf:
    sp = SpawnedProc()
    ghoti_pid = sp.spawn.pid
    sp.expect_prompt()
    sp.sendline(
        "function myexit --on-event ghoti_exit; /bin/echo $ghoti_pid > {filename}; end".format(
            filename=tf.name
        )
    )
    sp.expect_prompt()
    os.kill(ghoti_pid, signal.SIGHUP)
    # Note that ghoti's SIGHUP handling in interactive mode races with the call to select.
    # So actually close its stdin, like a terminal emulator would do.
    sp.spawn.close(force=False)
    sp.spawn.wait()
    tf.seek(0)
    line = tf.readline().strip()
    if line != str(ghoti_pid):
        colors = sp.colors()
        print(
            """{RED}Failed to find pid written by exit handler{RESET}""".format(
                **colors
            )
        )
        print("""Expected '{}', found '{}'""".format(ghoti_pid, line))
