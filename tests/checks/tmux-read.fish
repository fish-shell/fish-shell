#RUN: %fish %s
#REQUIRES: command -v tmux
#REQUIRES: test -z "$CI"

isolated-tmux-start -C '
    function fish_greeting
        set -l name (read)
        echo hello $name
    end
'

isolated-tmux send-keys name Enter 'echo foo' Enter
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: read> name
# CHECK: hello name
# CHECK: prompt 0> echo foo
# CHECK: foo
# CHECK: prompt 1>
