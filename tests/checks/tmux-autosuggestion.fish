#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start
isolated-tmux send-keys 'echo "foo bar baz"' Enter C-l
isolated-tmux send-keys 'echo '
tmux-sleep
isolated-tmux send-keys M-Right
isolated-tmux capture-pane -p
# CHECK: prompt 1> echo "foo bar baz"
tmux-sleep

touch COMPL

# Regression test.
isolated-tmux send-keys C-u C-l ': sometoken' M-b c
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 1> : csometoken

# Test that we get completion autosuggestions also when the cursor is not at EOL.
isolated-tmux send-keys C-u 'complete nofilecomp -f' Enter C-l 'nofilecomp ./CO' C-a M-d :
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 2> : ./COMPL

isolated-tmux send-keys C-u C-k C-l ': ./CO'
tmux-sleep
isolated-tmux send-keys A C-h
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 2> : ./COMPL

isolated-tmux send-keys C-u 'ech {' Left Left
tmux-sleep
isolated-tmux send-keys o C-e C-h 'still alive' Enter
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt {{\d+}}> echo still alive
# CHECK: still alive
# CHECK: prompt {{\d+}}>

isolated-tmux send-keys C-u 'echo (echo)' Enter
isolated-tmux send-keys C-l 'echo ('
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt {{\d+}}> echo (echo)

isolated-tmux send-keys C-u 'echo İ___' Enter C-l 'echo i'
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt {{\d+}}> echo İ___

isolated-tmux send-keys C-u 'echo İnstall' Enter C-l 'echo i'
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt {{\d+}}> echo İnstall
isolated-tmux send-keys n
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt {{\d+}}> echo in

touch some-file
isolated-tmux send-keys C-u C-l ': some-'
tmux-sleep
isolated-tmux send-keys f Tab
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt {{\d+}}> : some-file
