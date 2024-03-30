#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start

# Test moving around with up-or-search on a multi-line commandline.
isolated-tmux send-keys 'echo 12' M-Enter 'echo ab' C-p 345 C-n cde
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 0> echo 12345
# CHECK: echo abcde

isolated-tmux send-keys C-c
tmux-sleep
isolated-tmux send-keys C-l
isolated-tmux send-keys begin Enter 'echo 1' Enter e n d C-p 23
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 0> begin
# CHECK: echo 123
# CHECK: end

# regression test
isolated-tmux send-keys C-c # not sure why we need to wait after this
tmux-sleep
isolated-tmux send-keys 'bind S begin-selection' Enter C-l
tmux-sleep
isolated-tmux send-keys 'echo one two threeS' C-u C-y
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 1> echo one two three
