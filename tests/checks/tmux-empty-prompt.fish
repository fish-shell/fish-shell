#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start -C '
    function fish_prompt; end
    function fish_right_prompt
        set -q right_prompt
        and echo right-prompt
    end
    set right_prompt 1
    bind ctrl-g "set right_prompt 1" repaint
    bind alt-g "set -e right_prompt" repaint
'

isolated-tmux send-keys M-g C-l Enter
tmux-sleep
isolated-tmux send-keys C-g Enter
tmux-sleep
isolated-tmux capture-pane -p | string replace -r '$' '+'
#CHECK: +
#CHECK: right-prompt+
#CHECK: right-prompt+
#CHECK: +
#CHECK: +
#CHECK: +
#CHECK: +
#CHECK: +
#CHECK: +
#CHECK: +

isolated-tmux send-keys M-g Tab Tab
tmux-sleep
isolated-tmux capture-pane -p | string replace -r '$' '+'
#CHECK: +
#CHECK: {{.*}}+
#CHECK: {{.*}}+
#CHECK: {{.*}}+
#CHECK: {{.*}}+
#CHECK: {{.*}}+
#CHECK: {{.*}}+
#CHECK: {{.*}}+
#CHECK: {{.*}}+
#CHECK: {{.*}}+

isolated-tmux send-keys '
    function fish_prompt
        echo left-prompt\n
    end
'
tmux-sleep
isolated-tmux send-keys C-l Enter
tmux-sleep
isolated-tmux send-keys C-g Enter
tmux-sleep
tmux-sleep
tmux-sleep
isolated-tmux capture-pane -p | string replace -r '$' '+'
#CHECK: left-prompt+
#CHECK: +
#CHECK: left-prompt+
#CHECK: right-prompt+
#CHECK: left-prompt+
#CHECK: right-prompt+
#CHECK: +
#CHECK: +
#CHECK: +
#CHECK: +

isolated-tmux send-keys M-g Tab Tab
tmux-sleep
isolated-tmux capture-pane -p | string replace -r '$' '+'
#CHECK: left-prompt+
#CHECK: +
#CHECK: {{.*}}+
#CHECK: {{.*}}+
#CHECK: {{.*}}+
#CHECK: {{.*}}+
#CHECK: {{.*}}+
#CHECK: {{.*}}+
#CHECK: {{.*}}+
#CHECK: {{.*}}+
