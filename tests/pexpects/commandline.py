#!/usr/bin/env python3
from pexpect_helper import SpawnedProc, control
from re import escape

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

# Setup commandline test bindings
sendline(r"""function query_commandline;\
                 printf '\n%s ' (commandline --cursor); commandline;\
                 commandline -f cancel-commandline;\
             end;\
             bind \eQ query_commandline;\
             bind \eT test_commandline;""")
expect_prompt()

# Run a commandline test
#   commandline is the command line text
#   binding is the function body for test_commandline
#   expect is the expected value of $result (see query_commandline above)
def t(commandline, binding, expect):
    sendline(f"function test_commandline; {binding}; end")
    expect_prompt()
    sendline(f"{commandline}\x1bT\x1bQ")
    expect_prompt(escape(f"\r\n{expect}\r\n"))

# commandline --cursor
t("abcdef", "commandline --cursor 3", "3 abcdef")
t("abc defghi jkl",
  "commandline --cursor 8; commandline --current-token --cursor 1",
  "5 abc defghi jkl")
t("abc def", "commandline --current-token --cursor -- -3", "1 abc def")
t("abc def", "commandline --current-token --cursor -- -100", "0 abc def")
t("abc def",
  "commandline --current-token --cursor 0; commandline --insert x",
  "5 abc xdef")
t("echo ===; echo hello | echo there",
  "commandline --current-job --cursor 5",
  "14 echo ===; echo hello | echo there" )
t("echo hello | echo there",
  "commandline --current-process --cursor 4",
  "16 echo hello | echo there")

# commandline --insert
t("abc", "commandline --cursor 2; commandline --insert x", "3 abxc")
t("abc defghi jkl",
  "commandline --cursor 8; commandline --current-token --insert x",
  "9 abc defgxhi jkl")

# commandline --append
t("abc def; ghi", "commandline --append x", "12 abc def; ghix")
t("abc def ghi",
  "commandline --cursor 5; commandline --current-token --append x",
  "5 abc defx ghi")
t("abc def | efg hij",
  "commandline --cursor 1; commandline --current-process --append x",
  "1 abc def x| efg hij")
t("echo ===; echo hello | echo there; echo bye",
  "commandline --cursor 12; commandline --current-job --append x",
  "12 echo ===; echo hello | echo therex; echo bye")

# commandline --replace
t("abc def; ghi", "commandline --replace 'hello'", "5 hello")
t("abc def ghi",
  "commandline --cursor 5; commandline --current-token --replace xxx",
  "7 abc xxx ghi")
t("abc def | efg hij",
  "commandline --cursor 2; commandline --current-process --replace xxx",
  "3 xxx| efg hij")
t("echo ===; echo hello | echo there; echo bye",
  "commandline --cursor 12; commandline --current-job --replace xxx",
  "12 echo ===;xxx; echo bye")

sendline("bind '~' 'handle_tilde'")
expect_prompt()

# printing the current buffer should not remove quoting
sendline(
    "function handle_tilde; echo; echo '@GUARD:1@'; commandline -b; echo '@/GUARD:1@'; commandline -b ''; end"
)
expect_prompt()
sendline("echo \en one \"two three\" four'five six'{7} 'eight~")
expect_prompt("\r\n@GUARD:1@\r\n(.*)\r\n@/GUARD:1@\r\n")

# printing the buffer with -o should remove quoting
sendline(
    "function handle_tilde; echo; echo '@GUARD:2@'; commandline -bo; echo '@/GUARD:2@'; commandline -b ''; end"
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
expect_str("foo bar foo\ bar\ ")
send("\b" * 64)

# Commandline works when run on its own (#8807).
sendline("commandline whatever")
expect_re("prompt [0-9]+>whatever")

# Test --current-process output
send(control("u"))
sendline(r"bind \cb 'set tmp (commandline --current-process)'")
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

sendline(r"bind \cb 'set tmp (commandline --current-process | count)'")
sendline(r'commandline "echo line1 \\" "# comment" "line2"')
send(control("b"))
send(control("u") * 6)
sendline('echo "process spans $tmp lines"')
expect_str("process spans 3 lines")
