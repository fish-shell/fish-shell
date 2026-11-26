#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start -C '
    set -gx TERM_PROGRAM Apple_Terminal
    set -g fish_autosuggestion_enabled 0
    function __fish_winch_handler --on-signal WINCH
        commandline -f repaint >/dev/null 2>/dev/null
    end
    function fish_prompt
        printf "HEADER\\n> "
    end
    function fish_right_prompt
        printf "CLOCK"
    end
    bind ctrl-g "stty cols 120; commandline -f repaint"
'
isolated-tmux set-option -g status off
isolated-tmux resize-window -x 90 -y 20
tmux-sleep
isolated-tmux send-keys C-l
tmux-sleep
isolated-tmux clear-history

# Regression test: a resize can narrow the terminal before Fish receives its
# new width. Change only the PTY size so the right prompt overruns the pane.
isolated-tmux send-keys C-g
tmux-sleep
isolated-tmux display-message -p 'cursor row: #{cursor_y}'
# CHECK: cursor row: 1

# Once Fish learns the real width, the next repaint must reuse the header row.
isolated-tmux resize-window -x 91
tmux-sleep
set -l pane $(isolated-tmux capture-pane -p -S -)
printf 'headers: %s\n' "$(string match -ar HEADER -- $pane | count)"
# CHECK: headers: 1

# The right prompt must leave wrapping enabled for regular command output.
isolated-tmux send-keys 'printf "\x57%.0s" $(seq 1 100); echo' Enter
tmux-sleep
set pane $(isolated-tmux capture-pane -p -S -)
printf 'wrapped characters: %s\n' "$(string match -ar W -- $pane | count)"
# CHECK: wrapped characters: 100
