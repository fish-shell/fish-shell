#RUN: %fish %s
#REQUIRES: command -v tmux
# disable on github actions because it's flakey
#REQUIRES: test -z "$CI"

# The default history-delete binding is shift-delete which
# won't work on terminals that don't support CSI u, so rebind.
set -g isolated_tmux_fish_extra_args -C '
    set -g fish_autosuggestion_enabled 0
    bind alt-d history-delete or backward-delete-char
'
isolated-tmux-start

isolated-tmux send-keys 'true needle' Enter
# CHECK: prompt 0> true needle
tmux-sleep
isolated-tmux send-keys 'true hay ee hay' Enter
# CHECK: prompt 1> true hay ee hay
tmux-sleep
isolated-tmux send-keys C-p C-a M-f M-f M-f M-.
# CHECK: prompt 2> true hay needle hay
tmux-sleep
isolated-tmux capture-pane -p

isolated-tmux send-keys C-e C-u true Up Up Escape
tmux-sleep
isolated-tmux capture-pane -p | grep 'prompt 2'
# CHECK: prompt 2> true
isolated-tmux send-keys C-z _
tmux-sleep
isolated-tmux capture-pane -p | grep 'prompt 2'
# CHECK: prompt 2> _

# When history pager fails to find a result, copy the search field to the command line.
isolated-tmux send-keys C-e C-u C-r "echo no such command in history"
tmux-sleep
isolated-tmux send-keys Enter
# CHECK: prompt 2> echo no such command in history
isolated-tmux capture-pane -p | grep 'prompt 2'
isolated-tmux send-keys C-c

isolated-tmux send-keys C-r hay/shmay
isolated-tmux send-keys C-w C-h
isolated-tmux send-keys Enter
# CHECK: prompt 2> true hay ee hay
isolated-tmux capture-pane -p | grep 'prompt 2>'
isolated-tmux send-keys C-c

isolated-tmux send-keys 'echo 1' Enter 'echo 2' Enter 'echo 3' Enter
isolated-tmux send-keys C-l echo Up M-d
tmux-sleep
isolated-tmux capture-pane -p
#CHECK: prompt 5> echo 2
isolated-tmux send-keys C-c
tmux-sleep

isolated-tmux send-keys "echo sdifjsdoifjsdoifj" Enter
tmux-sleep
isolated-tmux capture-pane -p | grep "^sdifjsdoifjsdoifj\|prompt 6>"
# CHECK: sdifjsdoifjsdoifj
# CHECK: prompt 6>
isolated-tmux send-keys C-e C-u C-r
tmux-sleep
isolated-tmux send-keys "echo sdifjsdoifjsdoifj"
tmux-sleep
isolated-tmux send-keys M-d # alt-d
tmux-sleep
isolated-tmux capture-pane -p | grep "(no matches)"
# CHECK: (no matches)
isolated-tmux send-keys Enter C-e C-u "echo foo" Enter
tmux-sleep
isolated-tmux capture-pane -p | grep "^foo\|prompt 7>"
# CHECK: foo
# CHECK: prompt 7>
