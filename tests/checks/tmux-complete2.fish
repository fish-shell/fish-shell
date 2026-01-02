#RUN: %fish %s
#REQUIRES: command -v tmux
#REQUIRES: uname -r | grep -qv Microsoft
# cautiously disable because tmux-complete.fish is disabled
#REQUIRES: test -z "$CI"

isolated-tmux-start

isolated-tmux send-keys 'touch ~/"path with spaces"' Enter
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 0> touch {{.*}}
# CHECK: prompt 1>

isolated-tmux send-keys C-l 'cat ~/space' Tab
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 1> cat ~/path\ with\ spaces

# Clear screen.
isolated-tmux send-keys C-c
tmux-sleep

isolated-tmux send-keys '
    set -g fish_autosuggestion_enabled 0
    set -l FISH_TEST_VAR_1 /
    set -l FISH_TEST_VAR_2 /
' Enter C-l
tmux-sleep
isolated-tmux capture-pane -p
# Note we keep prompt 1 because the above "set" commands don't bump $status_generation.
# CHECK: prompt 1>

isolated-tmux send-keys 'echo $FISH_TEST_v' Tab
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 1> echo $FISH_TEST_VAR_
# CHECK: …TEST_VAR_1  (Variable: /)  …TEST_VAR_2  (Variable: /)

mkdir -p clang/include
touch clang/INSTALL.txt
isolated-tmux send-keys C-u ': clang/in' Tab
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 1> : clang/include/
# CHECK: clang/INSTALL.txt  …/

mkdir -p clang/include-2
isolated-tmux send-keys C-u ': clang/in' Tab
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 1> : clang/include
# CHECK: clang/INSTALL.txt  …/include/  …/include-2/

isolated-tmux send-keys C-u 'complete truncate_custom_compl -a "Foo/bar/baz/{111,222,333}"' \
    Enter C-l 'truncate_custom_compl foo' Tab
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 2> truncate_custom_compl Foo/bar/baz/
# CHECK: …/111  …/222  …/333
