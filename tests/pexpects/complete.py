#!/usr/bin/env python3
from pexpect_helper import SpawnedProc

sp = SpawnedProc()
send, sendline, sleep, expect_prompt, expect_re = (
    sp.send,
    sp.sendline,
    sp.sleep,
    sp.expect_prompt,
    sp.expect_re,
)
expect_prompt()

sendline(
    """
    complete -c my_is -n 'test (count (commandline -opc)) = 1' -xa arg
    complete -c my_is -n '__fish_seen_subcommand_from not' -xa '(
	set -l cmd (commandline -opc) (commandline -ct)
	set cmd (string join " " my_is $cmd[3..-1])" "
	commandline --replace --current-process $cmd
	complete -C"$cmd"
    )'
"""
)

send("my_is not \t")
send("still.alive")
expect_re(".*still.alive")
