#!/usr/bin/env python3
import os
from pexpect_helper import SpawnedProc
import signal

sp = SpawnedProc()
send, sendline, sleep, expect_str, expect_prompt = sp.send, sp.sendline, sp.sleep, sp.expect_str, sp.expect_prompt
expect_prompt()

# Verify that cancel-commandline does what we expect - see #7384.
send("not executed")
sleep(0.05)
os.kill(sp.spawn.pid, signal.SIGINT)
sp.expect_str("not executed^C")
expect_prompt(increment=False)

sendline("function cancelhandler --on-event fish_cancel ; echo yay cancelled ; end")
expect_prompt()
send("still not executed")
sleep(0.05)
os.kill(sp.spawn.pid, signal.SIGINT)
expect_str("still not executed^C")
expect_prompt("yay cancelled", increment=False)
