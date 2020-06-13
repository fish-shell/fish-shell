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

# Test job summary for interactive shells.
expect_prompt()

sendline("function fish_job_summary; string join ':' $argv; end")
expect_prompt()

# fish_job_summary is called when background job ends.
sendline("sleep 0.5 &")
sleep(0.050)
expect_prompt()
sleep(0.550)
expect_re("[0-9]+:0:sleep 0.5 &:ENDED")
sendline("")
expect_prompt()

# fish_job_summary is called when background job is signalled.
# cmd_line correctly prints only the actually backgrounded job.
sendline("false; sleep 10 &; true")
sleep(0.100)
expect_prompt()
sendline("kill -TERM $last_pid")
sleep(0.100)
expect_re("[0-9]+:0:sleep 10 &:SIGTERM:Polite quit request")
sendline("")
expect_prompt()

# fish_job_summary is called when foreground job is signalled.
# cmd_line contains the entire pipeline. proc_id and proc_name are set in a pipeline.
sendline("true | sleep 6")
sleep(0.100)
call(["pkill", "-KILL", "sleep", "-P", str(sp.spawn.pid)])
sleep(0.100)
expect_re("[0-9]+:1:true|sleep 6:SIGKILL:Forced quit:[0-9]+:sleep")
