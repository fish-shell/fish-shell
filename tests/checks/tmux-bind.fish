#RUN: %fish %s
#REQUIRES: command -v tmux
# Github Actions currently (2022-06-09)
# doesn't include the tmux-256color terminfo on macOS.
# So we skip tmux tests there.
#REQUIRES: test -z "$CI" -o "$(uname)" != Darwin

isolated-tmux-start

# Test moving around with up-or-search on a multi-line commandline.
isolated-tmux send-keys 'echo 12' M-Enter 'echo ab' C-p 345 C-n cde
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 0> echo 12345
# CHECK: echo abcde
