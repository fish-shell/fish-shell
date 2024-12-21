#!/usr/bin/env python3
from pexpect_helper import SpawnedProc, TO_END

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

# Fish should start in default-mode (i.e., emacs) bindings. The default escape
# timeout is 30ms.
#
# Because common CI systems are awful, we have to increase this:

sendline("set -g fish_escape_delay_ms 120")
expect_prompt("")

# Validate standalone behavior
sendline("status current-commandline")
expect_prompt(TO_END + "status current-commandline\r\n")

# Validate behavior as part of a command chain
sendline("true 7 && status current-commandline")
expect_prompt(TO_END + "true 7 && status current-commandline\r\n")

# Validate behavior when used in a function
sendline("function report; set -g last_cmdline (status current-commandline); end")
expect_prompt("")
sendline("report 27")
expect_prompt("")
sendline("echo $last_cmdline")
expect_prompt(TO_END + "report 27\r\n")

# Exit
send("\x04")  # <c-d>
expect_str("")
