#RUN: %fish %s
#REQUIRES: command -v tmux

# Test moving around with up-or-search on a multi-line commandline.
isolated-tmux send-keys 'echo 12' M-Enter 'echo ab' C-p 345 C-n cde
$sleep
isolated-tmux capture-pane -p
# CHECK: prompt 0> echo 12345
# CHECK: echo abcde
