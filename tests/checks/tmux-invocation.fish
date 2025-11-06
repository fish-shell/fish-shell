#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start -C '
    set -g fish (status fish-path)
'

# Implicit interactive but output is redirected.
isolated-tmux send-keys \
    '$fish >output' Enter
tmux-sleep
isolated-tmux send-keys \
    'status is-interactive && printf %s i n t e r a c t i v e \n' Enter \
    C-d
tmux-sleep
# Extract the line where command output starts.
string match <output -r '.*\e\]133;C.*' |
string escape
# CHECK: {{.*}}interactive{{$}}

isolated-tmux send-keys \
    '$fish -c "read; cat"' Enter
tmux-sleep
isolated-tmux send-keys \
    'read-value ' Enter
tmux-sleep
isolated-tmux send-keys cat1 Enter
tmux-sleep
isolated-tmux send-keys cat2 Enter
tmux-sleep
isolated-tmux capture-pane -p -S 2
# CHECK: read> read-value
# CHECK: read-value cat1
# CHECK: cat1
# CHECK: cat2
# CHECK: cat2
