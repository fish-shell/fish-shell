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
