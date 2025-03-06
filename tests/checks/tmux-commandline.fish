#RUN: %fish %s
#REQUIRES: command -v tmux
# Somehow $LINES is borked on NetBSD?
#REQUIRES: test $(uname) != NetBSD
# Haunted under CI
#REQUIRES: test -z "$CI"

set -g isolated_tmux_fish_extra_args -C '
    bind ctrl-q "functions --erase fish_right_prompt" "commandline \'\'" clear-screen
    set -g fish_autosuggestion_enabled 0
    bind ctrl-g "__fish_echo commandline --current-job"
    bind ctrl-t \'__fish_echo echo cursor is at offset $(commandline --cursor --current-token) in token\'
'
isolated-tmux-start

isolated-tmux send-keys 'echo LINES $LINES' Enter
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 0> echo LINES $LINES
# CHECK: LINES 10
# CHECK: prompt 1>

isolated-tmux send-keys 'bind alt-g "commandline -p -C -- -4"' Enter C-l
isolated-tmux send-keys 'echo bar|cat' \eg foo
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 2> echo foobar|cat

isolated-tmux send-keys C-k C-u C-l 'commandline -i "\'$(seq $LINES)" scroll_here' Enter
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: 2
# CHECK: 3
# CHECK: 4
# CHECK: 5
# CHECK: 6
# CHECK: 7
# CHECK: 8
# CHECK: 9
# CHECK: 10
# CHECK: scroll_here

# Soft-wrapped commandline with omitted right prompt.
isolated-tmux send-keys C-q '
    function fish_right_prompt
        echo right-prompt
    end
    commandline -i "echo $(printf %0"$COLUMNS"d)"
' Enter
tmux-sleep
isolated-tmux capture-pane -p | sed 1,5d
# CHECK: prompt 5> echo 00000000000000000000000000000000000000000000000000000000000000000
# CHECK: 000000000000000
# CHECK: 00000000000000000000000000000000000000000000000000000000000000000000000000000000
# CHECK: prompt 6>                                                           right-prompt
