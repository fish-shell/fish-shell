#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start -C '
    set -g i 0
    function fish_prompt
        set -g i (math $i + 1)
        printf "$i.%s \n" (seq $i)
    end
    history append "\
if true
    echo hello1
    echo hello2
    echo hello3
    echo hello4
    echo hello5
    echo hello6
    echo hello7
    echo hello8
end"
'

isolated-tmux send-keys Enter
tmux-sleep
isolated-tmux send-keys i
tmux-sleep
isolated-tmux capture-pane -p | string replace -r ^ ^
# CHECK: ^1.1
# CHECK: ^2.1
# CHECK: ^2.2 if true
# CHECK: ^            echo hello1
# CHECK: ^            echo hello2
# CHECK: ^            echo hello3
# CHECK: ^            echo hello4
# CHECK: ^            echo hello5
# CHECK: ^            echo hello6
# CHECK: ^            echo hello7…
