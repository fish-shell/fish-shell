#RUN: %fish %s
#REQUIRES: command -v tmux
#REQUIRES: uname -r | grep -qv Microsoft

isolated-tmux-start -C 'set -g fish_autosuggestion_enabled 0'

# Join short options
isolated-tmux send-keys 'complete foo -f -a "-a\\\\tshort -b\\\\tshort"' Enter
tmux-sleep
isolated-tmux send-keys C-l 'foo ' Tab
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 1> foo -
# CHECK: -a{{ +}}-b{{ +}}(short)

# Join short and long options
isolated-tmux send-keys C-u 'complete -e foo; complete foo -f -a "-a\\\\tshort/long --long\\\\tshort/long"' Enter
tmux-sleep
isolated-tmux send-keys C-l 'foo ' Tab
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 2> foo -
# CHECK: -a{{ +}}--long{{ +}}(short/long)

# Join long options
isolated-tmux send-keys C-u 'complete -e foo; complete foo -f -a "--long1\\\\tlong --long2\\\\tlong"' Enter
tmux-sleep
isolated-tmux send-keys C-l 'foo ' Tab
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 3> foo --long
# CHECK: --long1{{ +}}--long2{{ +}}(long)

# Join command options
isolated-tmux send-keys C-u 'complete -e foo; complete foo -f -a "command_1st\\\\tmy\\ cmd command_2nd\\\\tmy\\ cmd"' Enter
tmux-sleep
isolated-tmux send-keys C-l 'foo ' Tab
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 4> foo command_
# CHECK: command_1st{{ +}}command_2nd{{ +}}(my cmd)

###
### Mix of options and commands
###
isolated-tmux send-keys C-u 'complete -e foo; complete foo -f -a "-a\\\\tdescription -b\\\\tdescription --long1\\\\tdescription --long2\\\\tdescription command_1st\\\\tdescription command_2nd\\\\tdescription"' Enter
tmux-sleep

### nothing => all options and commands
isolated-tmux send-keys C-l 'foo ' Tab
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 5> foo
# CHECK: command_1st{{ +}}command_2nd{{ +}}-a{{ +}}-b{{ +}}--long1{{ +}}--long2{{ +}}(description)

### `-` => short and long options
isolated-tmux send-keys C-u C-l 'foo -' Tab
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 5> foo -
# CHECK: -a{{ +}}-b{{ +}}--long1{{ +}}--long2{{ +}}(description)

### `--` => long options
isolated-tmux send-keys C-u C-l 'foo --' Tab
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 5> foo --long
# CHECK: --long1{{ +}}--long2{{ +}}(description)

### `c` => commands
isolated-tmux send-keys C-u C-l 'foo c' Tab
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 5> foo command_
# CHECK: command_1st{{ +}}command_2nd{{ +}}(description)
