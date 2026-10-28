#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start -C '
    set fish_transient_prompt 1
	function fish_prompt
		if contains -- --final-rendering $argv
			printf "\$ "
			return
		end
		echo "full-prompt> "
	end
    bind ctrl-g \'
        commandline --append " aaaaaa bbb cccccccccc dddddddddd eeee ffffffff ggggg"
        commandline -f execute
    \'
'
isolated-tmux resize-window -x 66 -y 1
tmux-sleep

isolated-tmux send-keys "echo eeeeee"
tmux-sleep
isolated-tmux send-keys C-g
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: full-prompt>
