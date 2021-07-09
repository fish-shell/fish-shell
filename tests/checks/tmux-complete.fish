#RUN: %fish -C 'set -g fish %fish' %s
#REQUIRES: command -v tmux

set fish (builtin realpath $fish)

# Isolated tmux.
set -g tmpdir (mktemp -d)

# Don't CD elsewhere, because tmux socket file is relative to CWD. Using
# absolute path to socket file is prone to 'socket file name too long' error.
cd $tmpdir

set -g tmux tmux -S .tmux-socket -f /dev/null

set -g sleep sleep .1
set -q CI && set sleep sleep 1


$tmux new-session -x 80 -y 10 -d $fish -C '
    # This is similar to "tests/interactive.config".
    function fish_greeting; end
    function fish_prompt; printf "prompt $status_generation> "; end
    # No autosuggestion from older history.
    set fish_history ""
'
$sleep # Let fish draw a prompt.

# Don't escape existing token (#7526).
echo >file-1
echo >file-2
$tmux send-keys 'HOME=$PWD ls ~/' Tab
$sleep
$tmux capture-pane -p
# Note the contents may or may not have the autosuggestion appended - it is a race.
# CHECK: prompt 0> HOME=$PWD ls ~/file-{{1?}}
# CHECK: ~/file-1  ~/file-2

# No pager on single smartcase completion (#7738).
$tmux send-keys C-u C-l 'mkdir cmake CMakeFiles' Enter C-l \
    'cat cmake' Tab
$sleep
$tmux capture-pane -p
# CHECK: prompt 1> cat cmake/

# Correct case in pager when prefixes differ in case (#7743).
$tmux send-keys C-u C-l 'complete -c foo2 -a "aabc aaBd" -f' Enter C-l \
    'foo2 A' Tab
$sleep
$tmux capture-pane -p
# The "bc" part is the autosuggestion - we could use "tmux capture-pane -e" to check colors.
# CHECK: prompt 2> foo2 aabc
# CHECK: aabc  aaBd

$tmux kill-server
rm -r $tmpdir
