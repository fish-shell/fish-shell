#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start
isolated-tmux send-keys 'set -g g_u_var foobar' Enter
tmux-sleep
isolated-tmux send-keys 'set -U g_u_var barfoo' Enter

tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 0> set -g g_u_var foobar
# CHECK: prompt 0> set -U g_u_var barfoo
# CHECK: set: successfully set universal 'g_u_var'; but a global by that name shadows it
# CHECK: prompt 0>
