#!/usr/bin/env python3
from pexpect_helper import SpawnedProc
import os
import pexpect
import signal
import sys

sp = SpawnedProc()
fish_pid = sp.spawn.pid
assert fish_pid is not None
sp.expect_prompt()

# Set up a fish_exit handler that prints a marker to stdout.
sp.sendline("function __test_exit --on-event fish_exit; echo SIGTERM_CLEANUP_OK; end")
sp.expect_prompt()

# Run a command to populate history.
sp.sendline("echo sigterm_history_test_%d" % fish_pid)
sp.expect_prompt()

# Send SIGTERM.
os.kill(fish_pid, signal.SIGTERM)

# Try to catch the cleanup marker before EOF.
idx = sp.spawn.expect(["SIGTERM_CLEANUP_OK", pexpect.EOF], timeout=5)
if idx == 1:
    print("FAIL: fish_exit did not fire on SIGTERM (no cleanup marker in output)")
    sys.exit(1)

# Drain remaining output.
sp.spawn.expect(pexpect.EOF, timeout=5)
sp.spawn.close()

# Verify death was by signal (re-raised SIGTERM), not normal exit.
if sp.spawn.signalstatus != signal.SIGTERM:
    print(
        "FAIL: expected SIGTERM, got signal=%s exit=%s"
        % (sp.spawn.signalstatus, sp.spawn.exitstatus)
    )
    sys.exit(1)

# Check history file for NUL bytes.
histfile = os.path.join(os.environ["XDG_DATA_HOME"], "fish", "fish_history")
with open(histfile, "rb") as f:
    raw = f.read()
assert len(raw) != 0, "history file is empty"
nul_count = raw.count(b"\x00")
if nul_count > 0:
    print("FAIL: history file contains %d NUL bytes" % nul_count)
    sys.exit(1)
