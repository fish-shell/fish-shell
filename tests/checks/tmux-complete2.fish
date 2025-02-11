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

isolated-tmux send-keys C-c '
    set -g fish_autosuggestion_enabled 0
    set -l FISH_TEST_VAR_1 /
    set -l FISH_TEST_VAR_2 /
' Enter C-l 'echo $FISH_TEST_v' Tab
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 2> echo $FISH_TEST_VAR_
# CHECK: $FISH_TEST_VAR_1  (Variable: /)  $FISH_TEST_VAR_2  (Variable: /)

