#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start

isolated-tmux send-keys \
    "sleep 0.25 &" Enter \
    "echo hello"
sleep .5
isolated-tmux send-keys Space world
isolated-tmux capture-pane -p
# CHECK: prompt 0> sleep 0.25 &
# CHECK: prompt 0> echo hello
# CHECK: fish: Job 1, 'sleep 0.25 &' has ended
# CHECK: prompt 0> echo hello world
