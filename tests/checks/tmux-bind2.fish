#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start

isolated-tmux send-keys "function prepend; commandline --cursor 0; commandline -i echo; end" Enter
isolated-tmux send-keys "bind ctrl-g prepend" Enter
isolated-tmux send-keys C-l
isolated-tmux send-keys 'printf'
isolated-tmux send-keys C-g Space
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 2> echo printf
