#RUN: fish=%fish %fish %s
#REQUIRES: command -v tmux
#REQUIRES: test -z "$CI"

isolated-tmux-start
isolated-tmux send-keys ': 1' Enter
isolated-tmux send-keys ': ' M-Up M-Down M-Up M-Up M-Up M-Down Enter
tmux-sleep
isolated-tmux send-keys 'echo still alive' Enter
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 0> : 1
# CHECK: prompt 1> : 1
# CHECK: prompt 2> echo still alive
# CHECK: still alive
# CHECK: prompt 3>

isolated-tmux send-keys 'complete : -xa "foobar foobaz"' Enter
tmux-sleep
isolated-tmux send-keys C-l ': fooba' Enter
tmux-sleep
isolated-tmux send-keys C-p Tab
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 4> : fooba
# CHECK: prompt 5> : fooba
# CHECK: foobar  foobaz

isolated-tmux send-keys C-u C-l 'read' Enter
tmux-sleep
isolated-tmux send-keys read-input Enter
tmux-sleep
isolated-tmux send-keys \
    'set fish_history' Enter \
    'true some command; read' Enter
tmux-sleep
isolated-tmux send-keys C-p
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 5> read
# CHECK: read> read-input
# CHECK: read-input{{⏎|¶|\^J}}
# CHECK: prompt 6> set fish_history
# CHECK: prompt 6> true some command; read
# CHECK: read> read-input
