#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start

isolated-tmux send-keys \
    "sleep 0.5 &" Enter
sleep 0.1
isolated-tmux send-keys \
    "echo hello"
sleep 0.6
isolated-tmux send-keys Space world
sleep 0.1
isolated-tmux capture-pane -p
# CHECK: prompt 0> sleep 0.5 &
# CHECK: prompt 0> echo hello
# CHECK: fish: Job 1, 'sleep 0.5 &' has ended
# (I've seen this print " world", I guess it depends on tmux version and how big it thinks the terminal is)
# CHECK: {{.*}} world

isolated-tmux send-keys C-u C-l "sleep 3 | cat &" Enter "bg %1" Enter
tmux-sleep
isolated-tmux capture-pane -p | string match '*to background*'
# CHECK: Send job 1 'sleep 3 | cat &' to background
