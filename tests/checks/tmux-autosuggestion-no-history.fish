#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start
isolated-tmux send-keys 'echo unique_history_command' Enter C-l

isolated-tmux send-keys 'echo uni'
tmux-sleep
isolated-tmux send-keys Tab
isolated-tmux capture-pane -p
# CHECK: prompt {{\d+}}> echo unique_history_command

touch complete_test_file
isolated-tmux send-keys C-u C-l ': complete_'
tmux-sleep
isolated-tmux send-keys Tab
isolated-tmux capture-pane -p
# CHECK: prompt {{\d+}}> : complete_test_file

isolated-tmux send-keys C-u 'set -gx fish_autosuggestion_include_history 0' Enter C-l

isolated-tmux send-keys 'echo uni'
tmux-sleep
isolated-tmux send-keys Tab
isolated-tmux capture-pane -p
# CHECK: prompt {{\d+}}> echo uni
# CHECK-NOT: prompt {{\d+}}> echo unique_history_command
