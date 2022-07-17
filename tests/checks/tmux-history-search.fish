#RUN: %fish %s
#REQUIRES: command -v tmux
# disable on github actions because it's flakey
#REQUIRES: test -z "$CI"

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
