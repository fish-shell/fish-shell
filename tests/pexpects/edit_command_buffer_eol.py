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
set -e VISUAL
set -gx EDITOR nvim
fish_default_key_bindings
ln -s -- "$(path resolve -- "$TMPDIR")" editor-temp
set -gx TMPDIR "$PWD/editor-temp"
mkdir editor-work
cd editor-work
function nvim
    set -a argv '+execute "file " . fnameescape(resolve(expand("%:p")))'
    set -a argv '+lua io.write("VIRTUALEDIT:", vim.fn.eval("&virtualedit"), "\n")'
    switch "$edit_mode"
        case end
            set -a argv '+normal! $'
        case insert
            set -a argv '+inoremap <F2> <Cmd>qall!<CR>' '+call feedkeys("a\<F2>", "xt")'
        case fail
            set -a argv '+normal! h' '+cquit 7'
    end
    command nvim --headless -u NONE -i NONE -n $argv '+qall!'
end
function fish_prompt
    printf 'EDITOR> '
end
bind ctrl-b 'set -e edit_mode; fish_vi_key_bindings; commandline -C 8'
bind -M insert -m default ctrl-b 'commandline -C 8'
bind ctrl-f 'set -g edit_mode fail'
bind ctrl-g 'commandline -C 9'
bind ctrl-t 'printf "\nCURSOR:%s\n" "$(commandline -C)"'
bind -M insert ctrl-t 'printf "\nCURSOR:%s\n" "$(commandline -C)"'
bind ctrl-r 'set -g edit_mode end; commandline -C 4'
bind ctrl-y 'set -g edit_mode insert; commandline -r "echo e"\u0301lan; commandline -C 8'
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


# Opening and quitting Neovim preserves the end position on a line with wide characters.
edit_and_expect_cursor(12)

# Preserve the default bindings' caret after wide characters.
sp.send(control("g"))
edit_and_expect_cursor(9)

# Neovim's native $ also becomes an insertion point after the last character.
sp.send(control("r"))
edit_and_expect_cursor(12)

# In Insert mode, keep the same position after a composing character.
sp.send(control("y"))
edit_and_expect_cursor(8)

# From Fish's vi Insert mode, preserve the caret through a Normal-mode edit.
sp.send(control("b"))
edit_and_expect_cursor(8)

# In vi Normal mode, keep the cursor on the character reported by Neovim.
sp.send(control("b"))
edit_and_expect_cursor(8)

# Apply the reported cursor position after a failed edit too.
sp.send(control("f"))
edit_and_expect_cursor(7)

# Return to the character before the Insert-mode caret.
sp.send(control("y"))
edit_and_expect_cursor(8)
sp.spawn.close()
