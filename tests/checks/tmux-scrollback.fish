#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start
isolated-tmux send-keys 'bind ctrl-g "commandline -f scrollback-push scrollback-push clear-screen"' Enter C-g
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 1>
