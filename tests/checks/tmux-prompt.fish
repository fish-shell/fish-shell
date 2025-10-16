#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start -C '
    function fish_prompt
        printf "prompt $status_generation> <status=$status> <$prompt_var> "
        set prompt_var ''
    end
    function on_prompt_var --on-variable prompt_var
        commandline -f repaint
    end
    function token-info
        __fish_echo echo "current token is <$(commandline -t)>"
    end
    bind ctrl-g token-info
'

isolated-tmux capture-pane -p
# CHECK: prompt 0> <status=0> <>

set -q CI && set sleep sleep 10
set -U prompt_var changed
tmux-sleep
isolated-tmux send-keys Enter
# CHECK: prompt 0> <status=0> <changed>

isolated-tmux send-keys echo Space 123
tmux-sleep
isolated-tmux send-keys C-g
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 0> <status=0> <> echo 123
# CHECK: current token is <123>
# CHECK: prompt 0> <status=0> <> echo 123

isolated-tmux send-keys C-u '
function fish_prompt
    printf "full line prompt\nhidden<----------------------------------------------two-last-characters-rendered->!!"
end' Enter C-l
isolated-tmux send-keys 'test "' Enter 'indent"'
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: full line prompt
# CHECK: …<----------------------------------------------two-last-characters-rendered->!!
# CHECK: test "
# CHECK: indent"

isolated-tmux send-keys C-c '
    function fish_prompt
        string repeat (math $COLUMNS) x
    end
' C-l 'echo hello'
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
# CHECK: echo hello

isolated-tmux send-keys C-c '
    function fish_prompt
        seq (math $LINES + 1)
    end
    function fish_right_prompt
        echo test
    end
' Enter
tmux-sleep
isolated-tmux capture-pane -p -S -11
# CHECK: 1
# CHECK: 2
# CHECK: 3
# CHECK: 4
# CHECK: 5
# CHECK: 6
# CHECK: 7
# CHECK: 8
# CHECK: 9
# CHECK: 10
# CHECK: 11                                                                          test
# CHECK: 2
# CHECK: 3
# CHECK: 4
# CHECK: 5
# CHECK: 6
# CHECK: 7
# CHECK: 8
# CHECK: 9
# CHECK: 10
# CHECK: 11                                                                          test
