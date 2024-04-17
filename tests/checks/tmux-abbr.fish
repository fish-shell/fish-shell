#RUN: %fish %s
#REQUIRES: command -v tmux

set -g isolated_tmux_fish_extra_args -C '
    set -g fish_autosuggestion_enabled 0
    function abbr-test
    end
    abbr -g abbr-test "abbr-test [expanded]"
'
isolated-tmux-start

# Expand abbreviations on space.
isolated-tmux send-keys abbr-test Space arg1 Enter
tmux-sleep
# CHECK: prompt {{\d+}}> abbr-test [expanded] arg1

# Expand abbreviations at the cursor when executing.
isolated-tmux send-keys abbr-test Enter
tmux-sleep
# CHECK: prompt {{\d+}}> abbr-test [expanded]

# Use Control+Z right after abbreviation expansion, to keep going without expanding.
isolated-tmux send-keys abbr-test Space C-z arg2 Enter
tmux-sleep
# CHECK: prompt {{\d+}}> abbr-test arg2

# Or use Control+Space ("bind -k nul") to the same effect.
isolated-tmux send-keys abbr-test C-Space arg3 Enter
tmux-sleep
# CHECK: prompt {{\d+}}> abbr-test arg3

# Do not expand abbrevation if the cursor is not at the command, even if it's just white space.
# This makes the behavior more consistent with the above two scenarios.
isolated-tmux send-keys abbr-test C-Space Enter
tmux-sleep
# CHECK: prompt {{\d+}}> abbr-test

# CHECK: prompt {{\d+}}>

isolated-tmux capture-pane -p
