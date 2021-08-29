function isolated-tmux --inherit-variable tmpdir
    set -l tmpdir (mktemp -d)
    cd $tmpdir

    set -g sleep sleep .1
    set -q CI && set sleep sleep 1

    # Evil hack - override ourselves, so we only run initialization once.
    # We could do this outside the function, but then initialization is done too early for a
    # command that wants to set $isolated_tmux_fish_extra_args first, like
    #   fish -C 'source tests/test_functions/isolated-tmux.fish' tests/checks/tmux-prompt.fish
    function isolated-tmux --inherit-variable tmpdir
        # tmux can't handle session sockets in paths that are too long, and macOS has a very long
        # $TMPDIR, so use a relative path - except macOS doesn't have `realpath --relative-to`...
        # Luckily, we don't need to call tmux from other directories, so just make sure no one
        # does by accident.
        if test $PWD != $tmpdir
            echo "error: isolated-tmux must always be run from the same directory." >&2
            return 1
        end
        tmux -S .tmux-socket -f /dev/null $argv
    end

    function isolated-tmux-cleanup --on-event fish_exit --inherit-variable tmpdir
        isolated-tmux kill-server
        rm -r $tmpdir
    end

    set -l fish (status fish-path)
    isolated-tmux new-session -x 80 -y 10 -d $fish -C '
        # This is similar to "tests/interactive.config".
        function fish_greeting; end
        function fish_prompt; printf "prompt $status_generation> "; end
        # No autosuggestion from older history.
        set fish_history ""
    ' $isolated_tmux_fish_extra_args
    # Set the correct permissions for the newly created socket to allow future connections.
    # This is required at least under WSL or else each invocation will return a permissions error.
    chmod 777 .tmux-socket
    $sleep # Let fish draw a prompt.

    if set -q argv[1]
        isolated-tmux $argv
    end
end
