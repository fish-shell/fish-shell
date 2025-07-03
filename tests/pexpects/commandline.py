#!/usr/bin/env python3
from pexpect_helper import SpawnedProc, control

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

# Test --showing-suggestion before we dirty the history
sendline("echo hello")
expect_prompt()
sendline(
    "function debug; commandline --showing-suggestion; set -g cmd_status $status; end"
)
expect_prompt()
sendline("bind ctrl-p debug")
expect_prompt()
send("echo hell")
sleep(0.1)  # wait for suggestion to appear under CI
send(control("p"))
sendline("")
expect_prompt("hell")
sendline("echo cmd_status: $cmd_status")
expect_prompt("cmd_status: 0")
send("echo goodb")
sleep(0.1)  # wait for suggestion to appear under CI
send(control("p"))
sendline("")
expect_prompt("goodb")
sendline("echo cmd_status: $cmd_status")
expect_prompt("cmd_status: 1")

sendline("bind '~' 'handle_tilde'")
expect_prompt()

# printing the current buffer should not remove quoting
sendline(
    "function handle_tilde; echo; echo '@GUARD:1@'; commandline -b; echo '@/GUARD:1@'; commandline -b ''; end"
)
expect_prompt()
sendline("echo \\en one \"two three\" four'five six'{7} 'eight~")
expect_prompt("\r\n@GUARD:1@\r\n(.*)\r\n@/GUARD:1@\r\n")

# printing the buffer with -o should remove quoting
sendline(
    "function handle_tilde; echo; echo '@GUARD:2@'; commandline -bx; echo '@/GUARD:2@'; commandline -b ''; end"
)
expect_prompt()
sendline("echo one \"two three\" four'five six'{7} 'eight~")
expect_prompt("\r\n@GUARD:2@\r\n(.*)\r\n@/GUARD:2@\r\n")

# Check that we don't infinitely loop here.
sendline("function fish_mode_prompt; commandline -f repaint; end")
expect_prompt()

sendline("echo foo")
expect_prompt("foo")

# commandline is empty when a command is executed.
sendline("set what (commandline)")
expect_prompt()
sendline('echo "<$what>"')
expect_prompt("<>")

# Test for undocumented -I flag.
# TODO: consider removing.
sendline("commandline -I foo")
expect_prompt("foo")

# See that the commandline is updated immediately inside completions.
sendline("complete -c foo -xa '(commandline)'")
expect_prompt()
send("foo bar \t")
expect_str("foo bar 'foo bar '")
send("\b" * 64)

# Commandline works when run on its own (#8807).
sendline("commandline whatever")
expect_re("prompt [0-9]+>whatever")

# Test --current-process output
send(control("u"))
sendline(r"bind ctrl-b 'set tmp (commandline --current-process)'")
expect_prompt()
send("echo process1; echo process2")
send(control("a"))
send(control("b"))
send(control("k"))
sendline("echo first process is [$tmp]")
expect_str("first process is [echo process1]")

send("echo process # comment")
send(control("a"))
send(control("b"))
send(control("k"))
sendline('echo "process extent is [$tmp]"')
expect_str("process extent is [echo process # comment]")

sendline(
    """$fish -C 'commandline "sq 2; exit"; commandline --cursor 1; commandline -i e'"""
)
expect_str("seq 2")
send("\r")
expect_str("1\r\n2\r\n")

sendline("""$fish -C 'commandline 123; read'""")
expect_str("read> 123")
sendline("456; exit")
expect_str("123456")

# DISABLED because it keeps failing under ASAN
# sendline(r"bind ctrl-b 'set tmp (commandline --current-process | count)'")
# sendline(r'commandline "echo line1 \\" "# comment" "line2"')
# send(control("b"))
# send(control("u") * 6)
# sendline('echo "process spans $tmp lines"')
# expect_str("process spans 3 lines")
