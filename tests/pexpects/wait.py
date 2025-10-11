#!/usr/bin/env python3
from pexpect_helper import SpawnedProc

import os
import sys

# Disable under SAN - keeps failing because the timing is too tight
if "FISH_CI_SAN" in os.environ:
    sys.exit(0)

sp = SpawnedProc()
send, sendline, sleep, expect_prompt, expect_re, expect_str = (
    sp.send,
    sp.sendline,
    sp.sleep,
    sp.expect_prompt,
    sp.expect_re,
    sp.expect_str,
)
expect_prompt()

# one background job
sendline("sleep .3 &")
expect_prompt()
sendline("wait")
expect_prompt("Job 1, 'sleep .3 &' has ended")
sendline("jobs")
expect_prompt("jobs: There are no jobs")

# three job ids specified
sendline("sleep 0.5 &; sleep 0.1 &; sleep 0.3 &; sleep 0.7 &; wait %1 %3 %4")
expect_str("Job 2, 'sleep 0.1 &' has ended")
expect_str("Job 3, 'sleep 0.3 &' has ended")
expect_str("Job 1, 'sleep 0.5 &' has ended")
expect_prompt("Job 4, 'sleep 0.7 &' has ended")
sendline("jobs")
expect_prompt("jobs: There are no jobs")

# specify job ids with -n option
sendline("sleep 0.5 &; sleep 0.1 &; sleep 0.3 &; wait -n %1 %3")
expect_str("Job 2, 'sleep 0.1 &' has ended")
expect_prompt("Job 3, 'sleep 0.3 &' has ended")
sendline("wait -n %1")
expect_prompt("Job 1, 'sleep 0.5 &' has ended")
sendline("jobs")
expect_prompt("jobs: There are no jobs")

# don't wait for stopped jobs
sendline("sleep 0.3 &")
expect_prompt()
sendline("kill -STOP %1")
expect_prompt()
sendline("wait")
expect_prompt()
sendline("wait %1")
expect_prompt()
sendline("wait -n")
expect_prompt()
sendline("bg %1")
expect_prompt()
sendline("wait")
expect_prompt()
sendline("jobs")
expect_prompt("jobs: There are no jobs")

sendline(
    "sleep .3 &; kill -STOP %1; kill -CONT %1; jobs | string match -r running; wait"
)
expect_prompt("running")

# return immediately when no jobs
sendline("wait")
expect_prompt()
sendline("wait -n")
expect_prompt()
sendline("jobs")
expect_prompt("jobs: There are no jobs")

# wait for jobs by its process name with -n option
sendline("for i in (seq 1 3); sleep 0.$i &; end")
expect_prompt()
sendline("wait -n sleep")
expect_prompt()
sendline("jobs | wc -l")
expect_str("2")
expect_prompt()
sendline("wait")
expect_prompt()
sendline("jobs")
expect_prompt("jobs: There are no jobs")

# complex case
sendline("for i in (seq 1 10); ls | sleep 0.2 | cat > /dev/null &; end")
expect_prompt()
sendline("sleep 0.3 | cat &")
expect_prompt()
sendline("sleep 0.1 &")
expect_prompt()
sendline("wait $last_pid sleep")
expect_prompt()
sendline("jobs")
expect_prompt("jobs: There are no jobs")

# don't wait for itself
sendline("wait wait")
expect_prompt("wait: Could not find child processes with the name 'wait'")
sendline("wait -n wait")
expect_prompt("wait: Could not find child processes with the name 'wait'")
sendline("jobs")
expect_prompt("jobs: There are no jobs")

# test with fish script
sendline("fish -c 'sleep 0.2 &; sleep 0.3 &; sleep 0.1 &; wait -n sleep; jobs | wc -l'")
expect_str("1")
expect_prompt()

# test error messages
sendline("wait 0")
expect_prompt("wait: '0' is not a valid process ID")
sendline("wait 1")
expect_prompt("wait: Could not find a job with process ID '1'")
sendline("wait hoge")
expect_prompt("wait: Could not find child processes with the name 'hoge'")

# See that we don't wait if job expansion fails
sendline("sleep 5m &")
expect_prompt()
sendline("wait %5")
expect_prompt("jobs: No suitable job: %5")
sendline("kill %1")
expect_prompt()
