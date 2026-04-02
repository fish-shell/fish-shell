#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start
isolated-tmux send-keys source Enter
isolated-tmux send-keys 'source -' Enter

tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 0> source
# CHECK: source: missing filename argument or input redirection
# CHECK: prompt 1> source -
# CHECK: prompt 1>
