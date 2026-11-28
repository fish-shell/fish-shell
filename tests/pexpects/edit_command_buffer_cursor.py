#!/usr/bin/env python3
from pexpect_helper import SpawnedProc, control


config = r"""
set -g fish_greeting
set -gx VISUAL test_editor
function test_editor
    return "$editor_status"
end
set -g editor_status 0
function fish_prompt
    printf 'EDITOR> '
end
fish_vi_key_bindings
bind -M insert ctrl-g 'commandline -r "echo edited"'
bind -M insert ctrl-t 'set -g editor_status 7; edit_command_buffer; printf "EXIT:%s\n" "$status"'
"""

sp = SpawnedProc(args=["--no-config", "-C", config])
sp.expect_str("EDITOR> ")
sp.send(control("g"))
sp.expect_str("echo edited")
sp.send(control("t"))
sp.expect_str("EXIT:7")
sp.spawn.close()
