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
# Set the correct permissions for the newly created socket to allow future connections.
# This is required at least under WSL or else each invocation will return a permissions error.
chmod 777 .tmux-socket
$sleep # Let fish draw a prompt.

# Test moving around with up-or-search on a multi-line commandline.
$tmux send-keys 'echo 12' M-Enter 'echo ab' C-p 345 C-n cde
$sleep
$tmux capture-pane -p
# CHECK: prompt 0> echo 12345
# CHECK: echo abcde

$tmux kill-server
rm -r $tmpdir
