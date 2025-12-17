function isolated-tmux-start --wraps fish
    set -l tmpdir (mktemp -d)
    cd $tmpdir

    echo 'set -g mode-keys emacs' >.tmux.conf

    function isolated-tmux --inherit-variable tmpdir --wraps tmux
        # tmux can't handle session sockets in paths that are too long, and macOS has a very long
        # $TMPDIR, so use a relative path - except macOS doesn't have `realpath --relative-to`...
        # Luckily, we don't need to call tmux from other directories, so just make sure no one
        # does by accident.
        if test $PWD != $tmpdir
            echo "error: isolated-tmux must always be run from the same directory." >&2
            return 1
        end
        tmux -S .tmux-socket -f .tmux.conf $argv
    end

    function isolated-tmux-cleanup --on-event fish_exit --inherit-variable tmpdir
        isolated-tmux kill-server
        rm -r $tmpdir
    end

    function tmux-sleep
        set -q CI && sleep 1
        or sleep 0.3
    end

    function sleep-until
        set -l cmd $argv[1]
        set -l i 0
        while [ $i -lt 100 ] && not eval "$cmd" >/dev/null
            tmux-sleep
            set i (math $i + 1)
        end
        if [ $i -eq 100 ]
            printf '%s\n' "timeout waiting for $cmd" >&2
            exit 1
        end
    end

    set -l fish (status fish-path)
    set -l size -x 80 -y 10
    isolated-tmux new-session $size -d $fish -C '
        # This is similar to "tests/interactive.config".
        function fish_greeting; end
        function fish_prompt; printf "prompt $status_generation> "; end
        # No autosuggestion from older history.
        set fish_history ""
        # No transient prompt.
        set fish_transient_prompt 0
        set -g CDPATH
    ' $argv
    # Set the correct permissions for the newly created socket to allow future connections.
    # This is required at least under WSL or else each invocation will return a permissions error.
    chmod 777 .tmux-socket

    # Resize window so we can attach to tmux session without changing panel size.
    isolated-tmux resize-window $size

    if test "$tmux_wait" = false
        return
    end

    # Loop a bit, until we get an initial prompt.
    for i in (seq 50)
        if test -n "$(isolated-tmux capture-pane -p)"
            return
        end
        sleep .2
    end
    echo "error: isolated-tmux-start timed out waiting for non-empty first prompt" >&2
end
