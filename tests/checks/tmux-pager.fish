#RUN: %fish %s
#REQUIRES: command -v tmux
#REQUIRES: test -z "$CI"

set -g isolated_tmux_fish_extra_args -C '
    history append ": $(string repeat $COLUMNS A)"
    complete : -a \'(seq (math "$LINES * $COLUMNS"))\'
'

isolated-tmux-start

# Check that completions don't break from truncated autosuggestion
isolated-tmux send-keys : Space Tab Tab
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: prompt 0> : AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA…
# CHECK: 1    135  269  403  537  671
# CHECK: 2    136  270  404  538  672
# CHECK: 3    137  271  405  539  673
# CHECK: 4    138  272  406  540  674
# CHECK: 5    139  273  407  541  675
# CHECK: 6    140  274  408  542  676
# CHECK: 7    141  275  409  543  677
# CHECK: 8    142  276  410  544  678
# CHECK: rows 1 to 8 of 134

# Check that completions don't break than called on empty line
isolated-tmux send-keys C-u 'set fish_autosuggestion_enabled 0; function fish_prompt; string repeat (math $COLUMNS - 2) 0; end' Enter
isolated-tmux send-keys C-l : Space Tab Tab
tmux-sleep
isolated-tmux capture-pane -p
# CHECK: 000000000000000000000000000000000000000000000000000000000000000000000000000000:
# CHECK:
# CHECK: 1    135  269  403  537  671
# CHECK: 2    136  270  404  538  672
# CHECK: 3    137  271  405  539  673
# CHECK: 4    138  272  406  540  674
# CHECK: 5    139  273  407  541  675
# CHECK: 6    140  274  408  542  676
# CHECK: 7    141  275  409  543  677
# CHECK: rows 1 to 7 of 134
