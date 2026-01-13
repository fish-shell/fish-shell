#!/usr/bin/env python3
from pexpect_helper import SpawnedProc, control

sp = SpawnedProc()
send, sendline, sleep, expect_prompt = (
    sp.send,
    sp.sendline,
    sp.sleep,
    sp.expect_prompt,
)


def use_suggestion(*, delay=0.1):
    sleep(delay)
    send("\033[C")
    sendline("")


def run(cmd: str):
    sendline(cmd)
    expect_prompt()


expect_prompt()

run("echo Hello")

# basic samecase history suggestion
send("echo He")
use_suggestion()
expect_prompt(">echo Hello\r\nHello")

# case-correcting history suggestion
send("echo he")
use_suggestion()
expect_prompt(">echo Hello\r\nHello")

# prefer samecase history suggestions, even if older
run("echo hello")
send("echo He")
use_suggestion()
expect_prompt(">echo Hello\r\nHello")

# case-correcting command suggestion
send("Tru")
use_suggestion(delay=2.0)
expect_prompt(">true \r\n")

# the motivating example: prefer icase history suggestions over icase completion suggestions
run("mkdir -p Projects/myproject Projects/wrongproject")

# (prerequisite: without any relevant history, and with more than one subdir, fish can't suggest deeper than Projects/)
send("cd pro")
use_suggestion(delay=0.5)
expect_prompt(">cd Projects/\r\n")

run("cd ..")

# (and now the actual test)
run("cd Projects/myproject")
run("cd ../..")

send("cd pro")
use_suggestion()
expect_prompt(">cd Projects/myproject\r\n")

run("cd ../..")

# BUT prefer samecase completion suggestions over icase history suggestions
run("mkdir problems")

send("cd pro")
use_suggestion(delay=0.5)
expect_prompt(">cd problems/\r\n")

send(control("c"))
run("touch configure && chmod +x configure")
run("echo clean &&\n./configure")
run("rm configure")
send("./con")
use_suggestion()
expect_prompt(">./con\r\n")

send(control("c"))
run("touch somecommand")
run("chmod +x somecommand")
run('echo "multiline-token\n./somecommand arg1 arg2"')
send("./some")
use_suggestion()
expect_prompt(">./somecommand \r\n")
