#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start
isolated-tmux send-keys breakpoint Enter

tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 0> breakpoint
# CHECK: breakpoint: Command not valid at an interactive prompt
# CHECK: prompt 1>
