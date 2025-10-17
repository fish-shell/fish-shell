#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start

# Smartcase completions keep insensitive matches available (#7944).
isolated-tmux send-keys C-u C-l 'rm -rf sc-smartcase' Enter \
    'mkdir sc-smartcase' Enter \
    'cd sc-smartcase' Enter \
    'mkdir dog Dodo' Enter C-l \
    'cd do' Tab
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt {{\d+}}> cd dog/
# CHECK: doDodo/  dog/
