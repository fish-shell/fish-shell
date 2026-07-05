#RUN: fish=%fish %fish %s
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

isolated-tmux send-keys C-u C-l "sleep 1 | cat &" Enter "bg %1" Enter
tmux-sleep
isolated-tmux capture-pane -p | string match '*to background*'
# CHECK: Send job 1 'sleep 1 | cat &' to background

sleep 1.5

isolated-tmux send-keys C-l '
    status job-control full
    for x in (seq 10)
        sleep 0.0001 &
        fg
    end
'
tmux-sleep
isolated-tmux capture-pane -p |
    string match -rv '^fg: There are no suitable jobs' |
    string match -rv '^killpg\(\d+, SIGCONT\): No such process$' |
    string match -rv '^Send job \d+ \(sleep 0.0001 &\) to foreground$' |
    string match -rv '^warning: Could not send job \d+ \(\'sleep 0.0001 &\'\) with pgid \d+ to foreground' |
    string match -rv '^tcsetpgrp: No such process'

# CHECK: prompt 3>
