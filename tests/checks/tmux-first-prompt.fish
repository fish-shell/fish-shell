#RUN: %fish %s
#REQUIRES: command -v tmux

tmux_wait=false \
    isolated-tmux-start -C '
    function fish_prompt
        command printf "%% "
    end
'

isolated-tmux send-keys 'echo hello'
tmux-sleep
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: % echo hello
