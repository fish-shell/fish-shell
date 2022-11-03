# SPDX-FileCopyrightText: © 2021 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#RUN: %fish %s
#REQUIRES: command -v tmux

isolated-tmux-start

# Don't escape existing token (#7526).
echo >file-1
echo >file-2
isolated-tmux send-keys 'HOME=$PWD ls ~/' Tab
tmux-sleep
isolated-tmux capture-pane -p
# Note the contents may or may not have the autosuggestion appended - it is a race.
# CHECK: prompt 0> HOME=$PWD ls ~/file-{{1?}}
# CHECK: ~/file-1  ~/file-2

# No pager on single smartcase completion (#7738).
isolated-tmux send-keys C-u C-l 'mkdir cmake CMakeFiles' Enter C-l \
    'cat cmake' Tab
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 1> cat cmake/

# Correct case in pager when prefixes differ in case (#7743).
isolated-tmux send-keys C-u C-l 'complete -c foo2 -a "aabc aaBd" -f' Enter C-l \
    'foo2 A' Tab
tmux-sleep
isolated-tmux capture-pane -p
# The "bc" part is the autosuggestion - we could use "capture-pane -e" to check colors.
# CHECK: prompt 2> foo2 aabc
# CHECK: aabc  aaBd

# Check that a larger-than-screen completion list does not stomp a multiline commandline (#8509).
isolated-tmux send-keys C-u 'complete -c foo3 -fa "(seq $LINES)\t(string repeat -n $COLUMNS d)"' Enter \
    C-l begin Enter foo3 Enter "echo some trailing line" \
    C-p C-e Space Tab Tab
tmux-sleep
isolated-tmux capture-pane -p | sed -n '1p;$p'
# Assert that we didn't change the command line.
# CHECK: prompt 3> begin
# Also ensure that the pager is actually fully disclosed.
# CHECK: rows 1 to {{\d+}} of {{\d+}}

# Canceling the pager removes the inserted completion, no mater what happens in the search field.
# The common prefix remains because it is inserted before the pager is shown.
isolated-tmux send-keys C-c
tmux-sleep
isolated-tmux send-keys C-l foo2 Space BTab b BSpace b Escape
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 3> foo2 aa

# Check that down-or-search works even when the pager is not selected.
isolated-tmux send-keys C-u foo2 Space Tab
tmux-sleep
isolated-tmux send-keys Down
tmux-sleep
isolated-tmux capture-pane -p
# Also check that we show an autosuggestion.
# CHECK: prompt 3> foo2 aabc aabc
# CHECK: aabc{{ *}}aaBd

# Check that a larger-than-screen completion does not break down-or-search.
isolated-tmux send-keys C-u 'complete -c foo4 -f -a "
    a-long-arg-\"$(seq $LINES | string pad -c_ --width $COLUMNS)\"
    b-short-arg"' Enter C-l foo4 Space Tab Tab Down
tmux-sleep
isolated-tmux capture-pane -p | head -1
# The second one is the autosuggestion. Maybe we should turn them off for this test.
# TODO there should be a prefix ("prompt 4> foo4") but we fail to draw that in this case.
# CHECK: {{.*}} b-short-arg a-long-arg{{.*}}

# Check that completion pager followed by token search search inserts two separate tokens.
isolated-tmux send-keys C-u echo Space old-arg Enter C-l foo2 Space Tab Tab M-.
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 5> foo2 aabc old-arg
