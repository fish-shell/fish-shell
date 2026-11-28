#!/usr/bin/env python3
import shutil
import sys

from pexpect_helper import SpawnedProc, control


if not shutil.which("nvim"):
    sys.exit(127)

config = r"""
set -g fish_greeting
set -g fish_cursor_end_mode exclusive
set -g fish_autosuggestion_enabled 0
set -gx VISUAL nvim
set -gx TMPDIR "$(path resolve -- "$TMPDIR")"
mkdir editor-work
cd editor-work
function nvim
    set -a argv '+lua io.write("VIRTUALEDIT:", vim.fn.eval("&virtualedit"), "\n")'
    switch "$edit_mode"
        case end
            set -a argv '+normal! $'
        case insert
            set -a argv '+inoremap <F2> <Cmd>qall!<CR>' '+call feedkeys("i\<F2>", "xt")'
    end
    command nvim --headless -u NONE -i NONE -n $argv '+qall!'
end
function fish_prompt
    printf 'EDITOR> '
end
bind ctrl-g 'commandline -C 7'
bind ctrl-t 'printf "\nCURSOR:%s\n" "$(commandline -C)"'
bind ctrl-r 'set -g edit_mode end; commandline -C 4'
bind ctrl-y 'set -g edit_mode insert; commandline -r "echo e"\u0301lan; commandline -C 7'
"""

sp = SpawnedProc(args=["--no-config", "-C", config])
sp.expect_str("EDITOR> ")
sp.send("echo 界界trial")
sp.expect_str("echo 界界trial")


def edit_and_expect_cursor(offset):
    sp.send("\x1be")
    sp.expect_str("VIRTUALEDIT:\r\n")
    sp.expect_str("EDITOR> ")
    sp.send(control("t"))
    sp.expect_str(f"CURSOR:{offset}\r\n")


# Neovim's cursor lands on the last character of the line.
edit_and_expect_cursor(11)

# Keep the same character position after wide characters.
sp.send(control("g"))
edit_and_expect_cursor(7)

# Convert the byte column reported after Neovim's native $.
sp.send(control("r"))
edit_and_expect_cursor(11)

# In Insert mode, keep the same position after a composing character.
sp.send(control("y"))
edit_and_expect_cursor(7)
sp.spawn.close()
