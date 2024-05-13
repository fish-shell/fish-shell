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
    bind enter "set transient true; commandline -f repaint execute"
'

isolated-tmux-start

isolated-tmux send-keys 'echo foo' Enter
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: > echo foo
# CHECK: foo
# CHECK: > full prompt >
