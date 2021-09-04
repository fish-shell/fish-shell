#RUN: %fish %s
#REQUIRES: command -v tmux

set -g isolated_tmux_fish_extra_args -C '
    function fish_prompt; printf "prompt $status_generation> <$prompt_var> "; end
    function on_prompt_var --on-variable prompt_var
        commandline -f repaint
    end
'

# Start it up and loop a bit, until we get an initial prompt.
isolated-tmux
for i in seq 10
    if string match -q '*prompt*' (isolated-tmux capture-pane -p)
        break
    end
    sleep .5
end

isolated-tmux capture-pane -p
# CHECK: prompt 0> <>

set -q CI && set sleep sleep 10
set -U prompt_var changed
$sleep
isolated-tmux capture-pane -p
# CHECK: prompt 0> <changed>
