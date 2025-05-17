#RUN: %fish %s
#REQUIRES: command -v tmux

set -g isolated_tmux_fish_extra_args -C '
    function fish_prompt
        if set -q transient
            printf "> "
            set --erase transient
        else
            printf "> full prompt > "
        end
    end
    bind ctrl-j "set transient true; commandline -f repaint execute"
'

isolated-tmux-start

isolated-tmux send-keys 'echo foo' C-j
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: > echo foo
# CHECK: foo
# CHECK: > full prompt >

# Regression test for transient prompt with single-line prompts.
isolated-tmux send-keys C-u '
    set -g fish_transient_prompt 1
    function fish_prompt
        printf "\$ "
    end
' C-l
isolated-tmux send-keys Enter Enter
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: $
# CHECK: $
# CHECK: $

# Test that multi-line transient are properly cleared.
isolated-tmux send-keys C-u C-l '
    function fish_prompt
        if contains -- --final-rendering $argv
            printf "final line%d\n" 1 2
        else
            printf "transient line%d\n" 1 2
        end
    end
' C-l
isolated-tmux send-keys Enter
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: final line1
# CHECK: final line2
# CHECK: transient line1
# CHECK: transient line2
