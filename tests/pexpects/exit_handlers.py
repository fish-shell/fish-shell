#!/usr/bin/env python3
from pexpect_helper import SpawnedProc
import os
import signal
import tempfile

# Ensure that on-exit handlers run even if we get SIGHUP.
with tempfile.NamedTemporaryFile(mode="r", encoding="utf8") as tf:
    sp = SpawnedProc()
    fish_pid = sp.spawn.pid
    sp.expect_prompt()
    sp.sendline(
        "function myexit --on-event fish_exit; /bin/echo $fish_pid > {filename}; end".format(
            filename=tf.name
        )
    )
    sp.expect_prompt()
    os.kill(fish_pid, signal.SIGHUP)
    # Note that fish's SIGHUP handling in interactive mode races with the call to select.
    # So actually close its stdin, like a terminal emulator would do.
    sp.spawn.close(force=False)
    sp.spawn.wait()
    tf.seek(0)
    line = tf.readline().strip()
    if line != str(fish_pid):
        colors = sp.colors()
        print(
            """{RED}Failed to find pid written by exit handler{RESET}""".format(
                **colors
            )
        )
        print("""Expected '{}', found '{}'""".format(fish_pid, line))
