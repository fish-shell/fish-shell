#RUN: %fish %s
#REQUIRES: command -v tmux

set -g isolated_tmux_fish_extra_args -C '
    set -g fish_key_bindings fish_vi_key_bindings
'
isolated-tmux-start

isolated-tmux send-keys 'echo 124' Escape
tmux-sleep # disambiguate escape from alt
isolated-tmux send-keys v b y p i 3
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: [I] prompt 0> echo 1241234
