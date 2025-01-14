#!/usr/bin/env python3
from pexpect_helper import SpawnedProc

sp = SpawnedProc()
send, sendline, sleep, expect_prompt, expect_re, expect_str = (
    sp.send,
    sp.sendline,
    sp.sleep,
    sp.expect_prompt,
    sp.expect_re,
    sp.expect_str,
)

from time import sleep
from subprocess import call
import os
import signal
import sys

# Disable under CI - keeps failing because the timing is too tight
if "CI" in os.environ:
    sys.exit(127)

# Test job summary for interactive shells.
expect_prompt()

sendline("function fish_job_summary; string join ':' $argv; end")
expect_prompt()

# fish_job_summary is called when background job ends.
sendline("sleep 0.5 &")
expect_prompt()
expect_re("[0-9]+:0:sleep 0.5 &:ENDED", timeout=20)
sendline("")
expect_prompt()

# fish_job_summary is called when background job is signalled.
# cmd_line correctly prints only the actually backgrounded job.
sendline("false; sleep 20 &; true")
expect_prompt()
sendline("set -l my_pid $last_pid")
expect_prompt("")
sendline("jobs")
expect_re("Job.*Group.*(CPU)?.*State.*Command")
expect_re(".*running.*sleep 20 &")
expect_prompt()
sendline("echo $my_pid")
m = expect_re("\\d+\r\n")
expect_prompt()
os.kill(int(m.group()), signal.SIGTERM)
expect_re("[0-9]+:0:sleep 20 &:SIGTERM:Polite quit request", timeout=20)
sendline("")
expect_prompt()

# fish_job_summary is called when foreground job is signalled.
# cmd_line contains the entire pipeline. proc_id and proc_name are set in a pipeline.
sendline("true | sleep 6")
sleep(0.200)
# Beware: Mac pkill requires that the -P argument come before the process name,
# else the -P argument is ignored.
call(["pkill", "-KILL", "-P", str(sp.spawn.pid), "sleep"])
expect_re("[0-9]+:1:true|sleep 6:SIGKILL:Forced quit:[0-9]+:sleep", timeout=10)
