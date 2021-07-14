#RUN: %fish -C 'set -g fish %fish' %s
#REQUIRES: command -v tmux

set fish (builtin realpath $fish)

# Isolated tmux.
# Note $XDG_CONFIG_HOME typically has a leading double-dot,
# so our uvars file will leak across runs; therefore
# descend more deeply into the tmpdir.
set -g tmpdir (mktemp -d)/inner1/inner2/
mkdir -p $tmpdir

# Don't CD elsewhere, because tmux socket file is relative to CWD. Using
# absolute path to socket file is prone to 'socket file name too long' error.
cd $tmpdir

set -g tmux tmux -S .tmux-socket -f /dev/null

set -g sleep sleep .1
set -q CI && set sleep sleep 10

while set -e prompt_var
end

$tmux new-session -x 80 -y 10 -d $fish -C '
    # This is similar to "tests/interactive.config".
    function fish_greeting; end
    function fish_prompt; printf "prompt $status_generation> <$prompt_var> "; end
    # No autosuggestion from older history.
    set fish_history ""
    function on_prompt_var --on-variable prompt_var
        commandline -f repaint
    end
'

$sleep # Let fish draw a prompt.

$tmux capture-pane -p
# CHECK: prompt 0> <>

set -U prompt_var changed
$sleep
$tmux capture-pane -p
# CHECK: prompt 0> <changed>

$tmux kill-server
rm -r $tmpdir
