#RUN: %ghoti %s
#REQUIRES: command -v tmux

set -g isolated_tmux_ghoti_extra_args -C '
    function ghoti_prompt; printf "prompt $status_generation> <$prompt_var> "; end
    function on_prompt_var --on-variable prompt_var
        commandline -f repaint
    end
'

isolated-tmux-start

isolated-tmux capture-pane -p
# CHECK: prompt 0> <>

set -q CI && set sleep sleep 10
set -U prompt_var changed
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 0> <changed>
