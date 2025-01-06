#!/usr/bin/env python3
from pexpect_helper import SpawnedProc
import os

sp = SpawnedProc()
send, sendline, sleep, expect_prompt, expect_re, expect_str = (
    sp.send,
    sp.sendline,
    sp.sleep,
    sp.expect_prompt,
    sp.expect_re,
    sp.expect_str,
)

# We're going to use three history files, including the default, to verify
# that the fish_history variable works as expected.
# (using as a relative path to avoid interpolating $XDG_DATA_HOME)
default_histfile = "xdg_data_home/fish/fish_history"
my_histfile = "xdg_data_home/fish/my_history"
env_histfile = "xdg_data_home/fish/env_history"


def grephistfile(line, file):
    sendline("grep '^" + line + "' " + file)

# Verify that if we spawn fish with no fish_history env var it uses the
# default file.
expect_prompt()

# Verify that a command is recorded in the default history file.
cmd1 = "echo $fish_pid default histfile"
hist_line = "- cmd: " + cmd1
sendline(cmd1)
expect_prompt()

# TODO: Figure out why this `history --save` is only needed in one of the
# three Travis CI build environments and neither of my OS X or Ubuntu servers.
sendline("history --save")
expect_prompt()

grephistfile(hist_line, default_histfile)
expect_str(hist_line)
expect_prompt()

# Switch to a new history file and verify it is written to and the default
# history file is not written to.
cmd2 = "echo $fish_pid my histfile"
hist_line = "- cmd: " + cmd2
sendline("set fish_history my")
expect_prompt()
sendline(cmd2)
expect_prompt()

grephistfile(hist_line, my_histfile)
expect_str(hist_line)
expect_prompt()

# We expect this grep to fail to find the pattern and thus the expect_prompt
# block is inverted.
grephistfile(hist_line, default_histfile)
expect_prompt()

# Switch back to the default history file.
cmd3 = "echo $fish_pid default histfile again"
hist_line = "- cmd: " + cmd3
sendline("set fish_history default")
expect_prompt()
sendline(cmd3)
expect_prompt()

# TODO: Figure out why this `history --save` is only needed in one of the
# three Travis CI build environments and neither of my OS X or Ubuntu servers.
sendline("history --save")
expect_prompt()

grephistfile(hist_line, default_histfile)
expect_str(hist_line)
expect_prompt()

# We expect this grep to fail to find the pattern and thus the expect_prompt
# block is inverted.
sendline("grep '^" + hist_line + "' " + my_histfile)
expect_prompt()
grephistfile(hist_line, my_histfile)
expect_prompt()

# Verify that if we spawn fish with a HISTFILE env var it uses that file.
# Start by shutting down the previous shell.
sendline("jobs")
expect_prompt("jobs: There are no jobs")
sendline("exit")
sendline("exit")
sp.spawn.wait()

# Set the fish_history env var.
os.environ["fish_history"] = "env"

# Spawn a new shell.
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

# Verify that the new fish shell is using the fish_history value for history.
cmd4 = "echo $fish_pid env histfile"
hist_line = "- cmd: " + cmd4
sendline(cmd4)
expect_prompt()

grephistfile(hist_line, env_histfile)
expect_str(hist_line)
expect_prompt()

# We expect this grep to fail to find the pattern and thus the expect_prompt
# block is inverted.
grephistfile(hist_line, default_histfile)
expect_prompt()
