#RUN: %fish %s
#REQUIRES: command -v tmux
#REQUIRES: uname -r | grep -qv Microsoft

isolated-tmux-start

# Test that Tab recomputes completion if the list was empty.
isolated-tmux send-keys ': somedir' Tab
mkdir somedirectory
isolated-tmux send-keys Tab
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 0> : somedirectory/

# But not if the completion pager wasn't empty.
mkdir somedirectory2 somedirectory3
isolated-tmux send-keys Tab
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 0> : somedirectory/
