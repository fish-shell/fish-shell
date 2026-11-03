#!/usr/bin/env python3
from pexpect_helper import SpawnedProc
import os

cwd = os.getcwd()

# Write config 1 (based on the original config)
default_config = ""
with open(cwd + "/xdg_config_home/fish/conf.d/interactive.fish", "r") as orig:
    default_config = orig.read()

os.mkdir(cwd + "/my_fish_config")
with open(cwd + "/my_fish_config/config.fish", "w") as new:
    new.write(default_config)
    new.write("\n")
    new.write("function which_config; echo This is config 1; end")

# Write config 2 (based on the original config)
os.mkdir(cwd + "/my_other_fish_config")
with open(cwd + "/my_other_fish_config/config.fish", "w") as new:
    new.write(default_config)
    new.write("\n")
    new.write("function which_config; echo This is config 2; end")

env = os.environ.copy()

# Use absolute directory
env["FISH_CONFIG_DIR"] = cwd + "/my_fish_config"
sp = SpawnedProc(env=env)
sp.expect_prompt("")
sp.sendline("which_config")
sp.expect_prompt("This is config 1")
sp.sendline("echo $__fish_config_dir")
sp.expect_prompt(cwd + "/my_fish_config")

# Use relative directory
env["FISH_CONFIG_DIR"] = "my_other_fish_config"
sp = SpawnedProc(env=env)
sp.expect_prompt("")
sp.sendline("which_config")
sp.expect_prompt("This is config 2")
sp.sendline("echo $__fish_config_dir")
# Relative config dir is converted to absolute path
sp.expect_prompt(cwd + "/my_other_fish_config")
sp.sendline("cd my_fish_config")
sp.expect_prompt("")
sp.sendline("which_config")
sp.expect_prompt("This is config 2")
