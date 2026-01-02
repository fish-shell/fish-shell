#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start -C '
    function fish_right_prompt
        echo RP
    end
    history append "echo $(string repeat 100 a)"
'
isolated-tmux send-keys Enter e
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 0> {{ *}} RP
# CHECK: prompt 0> echo {{(a{65,})}}
# CHECK: {{(a{35,})}}

isolated-tmux send-keys C-h C-l "echo line1" M-Enter "e"
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 0> echo line1 {{ *}} RP
# CHECK:           echo {{(a{65,})}}
# CHECK: {{(a{35,})}}
