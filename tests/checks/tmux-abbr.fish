#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start -C '
    set -g fish_autosuggestion_enabled 0
    function abbr-test
    end
    abbr -g abbr-test "abbr-test [expanded]"
'

if { command -v less && ! less --version 2>&1 | grep BusyBox } >/dev/null
    isolated-tmux send-keys "abbr --help | sed 1q | less" Enter
    tmux-sleep
    isolated-tmux capture-pane -p
    isolated-tmux send-keys q
else
    echo '(END)'
    echo 'prompt 0> abbr --help | sed 1q | less'
    echo 'Documentation for abbr'
end
# CHECK: (END)
# CHECK: prompt 0> abbr --help | sed 1q | less
# CHECK: Documentation for abbr

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

# Same with a redundant space; it does not expand abbreviations.
isolated-tmux send-keys C-u abbr-test Space C-z Space arg2 Enter
tmux-sleep
# CHECK: prompt {{\d+}}> abbr-test  arg2

# Or use Control+Space to the same effect.
isolated-tmux send-keys abbr-test C-Space arg3 Enter
tmux-sleep
# CHECK: prompt {{\d+}}> abbr-test arg3

# Do not expand abbreviation if the cursor is not at the command, even if it's just white space.
# This makes the behavior more consistent with the above two scenarios.
isolated-tmux send-keys abbr-test C-Space Enter
tmux-sleep
# CHECK: prompt {{\d+}}> abbr-test

# CHECK: prompt {{\d+}}>

isolated-tmux capture-pane -p
