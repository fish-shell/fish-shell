#RUN: %fish %s
#REQUIRES: command -v tmux

set -g isolated_tmux_fish_extra_args -C '
    function fish_prompt; echo \'$ \'; end
    bind ctrl-g "commandline -i \'echo \'(printf %0(math \$COLUMNS - (string length \'\$ echo \'))d 0)"
'
isolated-tmux-start

isolated-tmux send-keys C-g Enter
tmux-sleep
isolated-tmux capture-pane -p | awk 'NR <= 4 {print NR ":" $0}'

# CHECK: 1:$ echo 0000000000000000000000000000000000000000000000000000000000000000000000000
# CHECK: 2:0000000000000000000000000000000000000000000000000000000000000000000000000
# CHECK: 3:$
# CHECK: 4:
