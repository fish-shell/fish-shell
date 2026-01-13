#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start
isolated-tmux send-keys "echo -n 12" Enter
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 0> echo -n 12
# CHECK: 12{{\u23CE|\^J|\u00b6|}}
# CHECK: prompt 1>
