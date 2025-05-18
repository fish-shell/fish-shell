#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start

isolated-tmux send-keys \
    'function fish_prompt; echo "prompt> "; end' Enter \
    'if true' Enter \
    "echo $(printf %050d)" Enter \
    "echo $(printf %0100d)" Enter \
    'e' 'n' 'd' Enter C-l

isolated-tmux send-keys 'if'
tmux-sleep
tmux-sleep
isolated-tmux capture-pane -p | sed /if/,/end/s/^/^/
# CHECK: ^prompt> if true
# CHECK: ^            echo 00000000000000000000000000000000000000000000000000
# CHECK: ^            echo 00000000000000000000000000000000000000000000000000000000000000…
# CHECK: ^
# CHECK: ^        end

# Enter does not invalidate autosuggestion.
isolated-tmux send-keys ' true' Enter
tmux-sleep
isolated-tmux capture-pane -p | sed /if/,/end/s/^/^/
# CHECK: ^prompt> if true
# CHECK: ^            echo 00000000000000000000000000000000000000000000000000
# CHECK: ^            echo 00000000000000000000000000000000000000000000000000000000000000…
# CHECK: ^
# CHECK: ^        end

# Autosuggestion is also computed after Enter.
isolated-tmux send-keys C-u C-u C-u 'if true' Enter
tmux-sleep
isolated-tmux capture-pane -p  \; send-keys C-u C-u C-u C-l | sed /if/,/end/s/^/^/
# CHECK: ^prompt> if true
# CHECK: ^            echo 00000000000000000000000000000000000000000000000000
# CHECK: ^            echo 00000000000000000000000000000000000000000000000000000000000000…
# CHECK: ^
# CHECK: ^        end

# Test smaller windows; only the lines that fit will be shown.
isolated-tmux send-keys 'if' \; resize-window -y 4
tmux-sleep
isolated-tmux capture-pane -p | sed s/^/^/
# CHECK: ^prompt> if true
# CHECK: ^            echo 00000000000000000000000000000000000000000000000000
# CHECK: ^            echo 00000000000000000000000000000000000000000000000000000000000000…
# CHECK: ^

# Currently, we take either all or nothing from soft-wrapped suggestion-lines.
# The ellipsis means that we'll get more lines.
isolated-tmux resize-window -y 3
tmux-sleep
isolated-tmux capture-pane -p | sed s/^/^/
# CHECK: ^prompt> if true
# CHECK: ^            echo 00000000000000000000000000000000000000000000000000…
# CHECK: ^

# Test that truncation also works after the resize.
isolated-tmux send-keys C-u if
tmux-sleep
isolated-tmux capture-pane -p | sed s/^/^/
# CHECK: ^prompt> if true
# CHECK: ^            echo 00000000000000000000000000000000000000000000000000…
# CHECK: ^

# Test that we truncate such that the prompt is never pushed up.
isolated-tmux resize-window -y 5 \; send-keys C-u Enter if
tmux-sleep
isolated-tmux capture-pane -p | sed s/^/^/
# CHECK: ^prompt>
# CHECK: ^prompt> if true
# CHECK: ^            echo 00000000000000000000000000000000000000000000000000
# CHECK: ^            echo 00000000000000000000000000000000000000000000000000000000000000…
# CHECK: ^

# Again, we take all or nothing from a soft-wrapped line.
isolated-tmux send-keys C-u Enter if
tmux-sleep
isolated-tmux capture-pane -p | sed s/^/^/
# CHECK: ^prompt>
# CHECK: ^prompt>
# CHECK: ^prompt> if true
# CHECK: ^            echo 00000000000000000000000000000000000000000000000000…
# CHECK: ^

# Now try with a multiline prompt.
isolated-tmux send-keys C-u 'function fish_prompt; printf "prompt-line%d/2> \n" 1 2; end' Enter C-l Enter if
tmux-sleep
isolated-tmux capture-pane -p | sed s/^/^/
# CHECK: ^prompt-line1/2>
# CHECK: ^prompt-line2/2>
# CHECK: ^prompt-line1/2>
# CHECK: ^prompt-line2/2> if true
# CHECK: ^                    echo 00000000000000000000000000000000000000000000000000…

isolated-tmux send-keys C-u \; resize-window -y 6 \; send-keys if
tmux-sleep
isolated-tmux capture-pane -p | sed s/^/^/
# CHECK: ^prompt-line1/2>
# CHECK: ^prompt-line2/2>
# CHECK: ^prompt-line1/2>
# CHECK: ^prompt-line2/2> if true
# CHECK: ^                    echo 00000000000000000000000000000000000000000000000000…
# CHECK: ^

isolated-tmux send-keys C-u \; resize-window -y 7 \; send-keys if
tmux-sleep
isolated-tmux capture-pane -p | sed s/^/^/
# CHECK: ^prompt-line1/2>
# CHECK: ^prompt-line2/2>
# CHECK: ^prompt-line1/2>
# CHECK: ^prompt-line2/2> if true
# CHECK: ^                    echo 00000000000000000000000000000000000000000000000000
# CHECK: ^                    echo 000000000000000000000000000000000000000000000000000000…
# CHECK: ^

# Autosuggestion with a line that barely wraps.
isolated-tmux resize-window -x 80 -y 4 \; send-keys C-u \
    'function fish_prompt; printf "prompt-line1\n> "; end' Enter \
    b e g i n Enter \
    # prompt=2 command=2 indent=4
    ": $(printf %072d)" Enter \
    Enter \
    Enter \
    Enter \
    e n d Enter C-l b e g i n
tmux-sleep
isolated-tmux capture-pane -p | sed s/^/^/
# CHECK: ^prompt-line1
# CHECK: ^> begin
# CHECK: ^      : 00000000000000000000000000000000000000000000000000000000000000000000000…
# CHECK: ^

# Autosuggestions on a soft-wrapped commandline don't push the prompt.
isolated-tmux resize-window -x 6 -y 4 \; send-keys C-u \
    'function fish_prompt; printf "> "; end' Enter \
    'echo l1 \\' Enter 'indented line continuation' Enter \
    C-l Enter 'e'
tmux-sleep
isolated-tmux capture-pane -p | sed s/^/^/
# CHECK: ^>
# CHECK: ^> ech…
# CHECK: ^
# CHECK: ^

isolated-tmux resize-window -x 6 -y 4 \; send-keys C-u \
    'function fish_prompt; printf "> "; end' Enter \
    'echo wrapped \\' Enter \
    'l1 \\' Enter \
    'l2 \\' Enter \
    'l3' Enter \
    Enter Enter \
    'echo'
tmux-sleep
isolated-tmux capture-pane -p | sed s/^/^/
# CHECK: ^>
# CHECK: ^>
# CHECK: ^> echo
# CHECK: ^ wrap…
