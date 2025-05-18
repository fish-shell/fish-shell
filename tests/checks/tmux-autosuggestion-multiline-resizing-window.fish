#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start -C '
    tmux resize-window -y 10
    history append "\
if true
    echo hello1
    echo hello2
    echo hello3
    echo hello4
    echo hello5
end"
'

isolated-tmux \
    send-keys (for i in (seq 9); echo Enter; end) \; \
    resize-window -y 5
tmux-sleep
isolated-tmux \
    resize-window -y 10
tmux-sleep
isolated-tmux \
    send-keys i
tmux-sleep
isolated-tmux capture-pane -p | string replace -r ^ ^
# CHECK: ^prompt 0>
# CHECK: ^prompt 0>
# CHECK: ^prompt 0>
# CHECK: ^prompt 0>
# CHECK: ^prompt 0>
# CHECK: ^prompt 0>
# CHECK: ^prompt 0>
# CHECK: ^prompt 0>
# CHECK: ^prompt 0>
# CHECK: ^prompt 0> if true…
