function isolated-tmux-start
    set -l tmpdir (mktemp -d)
    cd $tmpdir

    echo 'set -g mode-keys emacs' >.tmux.conf

    function isolated-tmux --inherit-variable tmpdir
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

    # Loop a bit, until we get an initial prompt.
    for i in (seq 25)
        if string match -q '*prompt*' -- (isolated-tmux capture-pane -p)
            break
        end
        sleep .2
    end
end
