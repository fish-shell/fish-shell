#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start -C '
    function fish_prompt; echo \'$ \'; end
    bind ctrl-g "commandline -i \'echo \'(printf %0(math \$COLUMNS - (string length \'\$ echo \'))d 0)"
'

isolated-tmux send-keys C-g Enter
tmux-sleep
isolated-tmux capture-pane -p | awk 'NR <= 4 {print NR ":" $0}'

# CHECK: 1:$ echo 0000000000000000000000000000000000000000000000000000000000000000000000000
# CHECK: 2:0000000000000000000000000000000000000000000000000000000000000000000000000
# CHECK: 3:$
# CHECK: 4:
