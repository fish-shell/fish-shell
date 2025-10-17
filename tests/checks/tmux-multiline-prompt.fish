#RUN: %fish %s
#REQUIRES: command -v tmux && ! tmux -V | grep -qE '^tmux (next-3.4|3\.[0123][a-z]*($|[.-]))'
#REQUIRES: command -v less && ! less --version 2>&1 | grep -q BusyBox
# disable on github actions because it's flakey
#REQUIRES: test -z "$CI"

isolated-tmux-start

isolated-tmux send-keys '
    function fish_prompt
        printf "prompt-line-1\\nprompt-line-2> "
        commandline -f repaint
    end
' Enter
isolated-tmux send-keys C-l ': 1' Enter
tmux-sleep
isolated-tmux send-keys ': 3' Enter
tmux-sleep
isolated-tmux send-keys ': 5' Enter
tmux-sleep

# Screen looks like

# [y=0] prompt-line-1
# [y=1] prompt-line-2> : 1
# [y=2] prompt-line-1
# [y=3] prompt-line-2> : 3
# [y=4] prompt-line-1
# [y=5] prompt-line-2> : 5
# [y=6] prompt-line-1
# [y=7] prompt-line-2>

isolated-tmux copy-mode
isolated-tmux send-keys -X previous-prompt
isolated-tmux send-keys -X previous-prompt
tmux-sleep
isolated-tmux display-message -p '#{copy_cursor_y} #{copy_cursor_line}'
# CHECK: {{[46]}} prompt-line-1

# Test that the prevd binding does not break the prompt.
isolated-tmux send-keys Escape
tmux-sleep
isolated-tmux send-keys M-left
tmux-sleep
isolated-tmux capture-pane -p | tail -n 5
# CHECK: prompt-line-2> : 5
# CHECK: prompt-line-1
# CHECK: prompt-line-2>
# CHECK:
# CHECK:

# Test repainting after running an external program that uses the alternate screen.
isolated-tmux send-keys "bind ctrl-r 'echo | less -+F -+X +q; commandline \"echo Hello World\"'" Enter C-l
isolated-tmux send-keys C-r
tmux-sleep
isolated-tmux send-keys Enter
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt-line-1
# CHECK: prompt-line-2> echo Hello World
# CHECK: Hello World
# CHECK: prompt-line-1
# CHECK: prompt-line-2>

# Test that transient prompt does not break the prompt.
isolated-tmux send-keys C-l "set fish_transient_prompt 1" Enter : Enter Enter
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt-line-1
# CHECK: prompt-line-2> set fish_transient_prompt 1
# CHECK: prompt-line-1
# CHECK: prompt-line-2> :
# CHECK: prompt-line-1
# CHECK: prompt-line-2>
# CHECK: prompt-line-1
# CHECK: prompt-line-2>
