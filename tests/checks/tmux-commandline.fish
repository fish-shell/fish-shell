#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start

isolated-tmux send-keys 'bind alt-g "commandline -p -C -- -4"' Enter C-l
isolated-tmux send-keys 'echo bar|cat' \eg foo
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 1> echo foobar|cat

isolated-tmux send-keys C-k C-u C-l 'commandline -i (seq $LINES) scroll_here' Enter
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: 2
# CHECK: 3
# CHECK: 4
# CHECK: 5
# CHECK: 6
# CHECK: 7
# CHECK: 8
# CHECK: 9
# CHECK: 10
# CHECK: scroll_here
