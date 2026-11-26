#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start -C '
    set -gx TERM_PROGRAM Apple_Terminal
    set -g fish_handle_reflow 1
    set -g fish_autosuggestion_enabled 0
    function __fish_winch_handler --on-signal WINCH
        commandline -f repaint >/dev/null 2>/dev/null
    end
    function fish_prompt
        printf "HEADER 0123456789012345678901234567890123456789012345678901234567890123456789\\n> "
    end
    function fish_right_prompt
        printf "12:34:56"
    end
    printf "SENTINEL-A\\nSENTINEL-B\\n"
'
isolated-tmux set-option -g status off
isolated-tmux resize-window -x 120 -y 30
tmux-sleep

function check_prompt
    set -l pane $(isolated-tmux capture-pane -p -S -)
    printf 'headers: %s\n' "$(string match -ar HEADER -- $pane | count)"
    printf 'prompts: %s\n' "$(string match -ar '> ' -- $pane | count)"
    printf 'sentinels: %s\n' "$(string match -ar 'SENTINEL-[AB]' -- $pane | count)"
    printf 'clocks: %s\n' "$(string match -ar '12:34:56' -- $pane | count)"
end

# Regression test: restoring a header hidden by a one-row window must not
# append a second copy of it below the original.
isolated-tmux resize-window -x 120 -y 1
tmux-sleep
isolated-tmux resize-window -x 120 -y 30
tmux-sleep
check_prompt
# CHECK: headers: 1
# CHECK: prompts: 1
# CHECK: sentinels: 2
# CHECK: clocks: 1

# Regression test: widening a reflowed multiline prompt must erase its old
# physical rows without erasing the preceding command output.
isolated-tmux resize-window -x 20 -y 30
tmux-sleep
isolated-tmux resize-window -x 120 -y 30
tmux-sleep
check_prompt
# CHECK: headers: 1
# CHECK: prompts: 1
# CHECK: sentinels: 2
# CHECK: clocks: 1
