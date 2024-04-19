#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start

isolated-tmux send-keys '
    function usr1_handler --on-signal SIGUSR1
        echo Got SIGUSR1
        # This repaint is not needed but make sure it is coalesced.
        commandline -f repaint
    end
' Enter
isolated-tmux send-keys C-l 'kill -SIGUSR1 $fish_pid' Enter
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 1> kill -SIGUSR1 $fish_pid
# CHECK: Got SIGUSR1
# CHECK: prompt 2>
