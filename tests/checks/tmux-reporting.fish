#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start \; \
    set-option -gw window-active-style 'bg=#f0f0f0' \; \
    send-keys ' cat -v' Enter
tmux-sleep
isolated-tmux capture-pane -p
# CHECK:  cat -v
# CHECK: prompt 0>  cat -v
