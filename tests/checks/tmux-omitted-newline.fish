#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start
isolated-tmux send-keys "echo -n 12" Enter
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 0> echo -n 12
# CHECK: 12{{⏎|\^J|¶|}}
# CHECK: prompt 1>

# Test that $fish_omitted_newline_indicator overrides the default.
isolated-tmux send-keys "set -g fish_omitted_newline_indicator NL; echo -n hello" Enter
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 0> echo -n 12
# CHECK: 12{{⏎|\^J|¶|}}
# CHECK: prompt 1> set -g fish_omitted_newline_indicator NL; echo -n hello
# CHECK: helloNL
# CHECK: prompt 2>

# Test that erasing the variable restores the platform default.
isolated-tmux send-keys "set -e fish_omitted_newline_indicator; echo -n bye" Enter
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 0> echo -n 12
# CHECK: 12{{⏎|\^J|¶|}}
# CHECK: prompt 1> set -g fish_omitted_newline_indicator NL; echo -n hello
# CHECK: helloNL
# CHECK: prompt 2> set -e fish_omitted_newline_indicator; echo -n bye
# CHECK: bye{{⏎|\^J|¶|}}
# CHECK: prompt 3>
