#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start

isolated-tmux send-keys 'bind alt-g "commandline -p -C -- -4"' Enter C-l
isolated-tmux send-keys 'echo bar|cat' \eg foo
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 1> echo foobar|cat
