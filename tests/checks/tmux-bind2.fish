#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start -C '
    bind alt-delete backward-kill-token
    bind alt-left backward-token
    bind alt-right forward-token
    set fish_autosuggestion_enabled 0
'

isolated-tmux send-keys "function prepend; commandline --cursor 0; commandline -i echo; end" Enter
isolated-tmux send-keys "bind ctrl-g prepend" Enter
isolated-tmux send-keys C-l
isolated-tmux send-keys 'printf'
isolated-tmux send-keys C-g Space
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 2> echo printf

isolated-tmux send-keys C-u C-k C-l 'echo ; foo &| ' M-delete 'bar | baz'
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 2> echo ; bar | baz

# To-do: maybe include the redirection?
isolated-tmux send-keys C-u C-l 'echo >ooba' M-left f M-right r
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 2> echo >foobar
