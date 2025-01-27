#RUN: %fish %s
#REQUIRES: command -v tmux
#REQUIRES: test -z "$CI"

isolated-tmux-start
isolated-tmux send-keys ': 1' Enter
isolated-tmux send-keys ': ' M-Up M-Down M-Up M-Up M-Up Enter
isolated-tmux send-keys C-l 'echo still alive' Enter
tmux-sleep
# isolated-tmux capture-pane -p | tail -2
isolated-tmux capture-pane -p
# CHECK: prompt 2> echo still alive
# CHECK: still alive
# CHECK: prompt 3>
