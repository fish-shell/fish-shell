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
expect_prompt()

# Verify COMPLETE_AUTO_SPACE behavior
sendline("complete -x -c monster -a truck")
expect_prompt()
sendline("complete -x -c monster -a energy=")
expect_prompt()
send("monster t")
sleep(0.1)
send("\t")
sleep(0.1)
send("!")
expect_str("monster truck !")  # space
send("\b" * 64)
send("monster e")
sleep(0.1)
send("\t")
sleep(0.1)
send("!")
expect_str("monster energy=!")  # no space
send("\b" * 64)

sendline(
    """
    # Make sure this function does nothing
    function my_is; :; end
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
sendline("")

# Check cancelling completion acceptance
# (bind cancel to something else so we don't have to mess with the escape delay)
sendline("bind \cg cancel")
sendline("complete -c echo -x -a 'foooo bar'")
send("echo fo\t")
send("\x07")
sendline("bar")
expect_re("bar")
sendline("echo fo\t")
expect_re("foooo")

# As soon as something after the "complete" happened,
# cancel should not undo.
# In this case that's the space after the tab!
send("echo fo\t ")
send("\x07")
sendline("bar")
expect_re("foooo bar")

sendline("bind \cg 'commandline -f cancel; commandline \"\"'")
send("echo fo\t")
expect_re("foooo")
send("\x07")
sendline("echo bar")
expect_re("\nbar")
sendline("echo fo\t")
expect_re("foooo")
