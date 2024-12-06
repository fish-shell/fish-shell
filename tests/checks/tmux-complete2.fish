#RUN: %fish %s
#REQUIRES: command -v tmux
#REQUIRES: uname -r | grep -qv Microsoft
# cautiously disable because tmux-complete.fish is disabled
#REQUIRES: test -z "$CI"

isolated-tmux-start

isolated-tmux send-keys 'touch ~/"path with spaces"' Enter C-l \
    'cat ~/space' Tab
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 1> cat ~/path\ with\ spaces
