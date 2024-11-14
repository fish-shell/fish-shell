#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start
isolated-tmux send-keys 'echo "foo bar baz"' Enter C-l
isolated-tmux send-keys 'echo '
tmux-sleep
isolated-tmux send-keys M-Right
isolated-tmux capture-pane -p
# CHECK: prompt 1> echo "foo bar baz"
tmux-sleep
