#!/usr/bin/env python3

import os
import signal

from pexpect_helper import SpawnedProc

env = os.environ.copy()
env["TERM"] = "xterm-kitty"
sp = SpawnedProc(dimensions=(6, 30), env=env)
sp.expect_prompt()
sp.send("# " + "x" * 240)
sp.expect_str("\x1b[?1000h")
sp.send("\x1b[<64;1;1M")
sp.expect_str("\x1b[?25l")

# A HUP normally means the PTY disappeared, but if it is still writable Fish
# must restore both viewport-specific and baseline reader protocols for a parent shell.
os.kill(sp.spawn.pid, signal.SIGHUP)
sp.expect_str("\x1b[?25h")
sp.expect_str("\x1b[?1000l")
sp.expect_str("\x1b[?1006l")
sp.expect_str("\x1b[?1016;1015;1006;1005;1003;1002;1001;1000;9r")
sp.expect_str("\x1b[?2004l")
sp.expect_str("\x1b[?2031l")
sp.expect_str("\x1b[>4;0m")
sp.expect_str("\x1b>")
sp.spawn.wait()
