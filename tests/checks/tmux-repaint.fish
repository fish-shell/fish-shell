#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start -C '
    function fish_prompt
        set -g counter (math $counter + 1)
        set -g fish_color_status red
        set -g fish_pager_color_background --background=white
        echo "$counter> "
        set -ga TERM .
        set -ga COLORTERM .
        set -ga fish_term256 .
        set -ga fish_term24bit .
    end
    set -eg fish_color_param
    bind ctrl-g,A  "{ set -l fish_color_command 111 }"
    bind ctrl-g,B "set -l fish_color_command 222"
    bind ctrl-g,C "set -e fish_color_command"
    bind ctrl-g,D "set -eg fish_color_param"
    bind ctrl-g,E "set -g fish_color_command 333"
    bind ctrl-g,F "set -U fish_color_param 444"
    bind ctrl-g,G "set -eg fish_color_command"
'

isolated-tmux capture-pane -p
# The weird global assignments in fish prompt cause an initial repaint.
# CHECK: 2>

isolated-tmux send-keys C-g A C-g B C-g C C-g D
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: 2>

isolated-tmux send-keys C-g E
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: 3>

isolated-tmux send-keys C-g F
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: 4>

isolated-tmux send-keys C-g G
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: 5>
