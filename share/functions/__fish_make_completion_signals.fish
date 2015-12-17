function __fish_make_completion_signals --description 'Make list of kill signals for completion'
    set -q __kill_signals; and return 0

    if kill -L ^/dev/null >/dev/null
        # Debian and some related systems use 'kill -L' to write out a numbered list
        # of signals. Use this to complete on both number _and_ on signal name.
        complete -c kill -s L --description "List codes and names of available signals"
        set -g __kill_signals (kill -L | sed -e 's/\([0-9][0-9]*\)  *\([A-Z,0-9][A-Z,0-9]*\)/\1 \2\n/g;s/ +/ /g' | sed -e 's/^ \+//' | __fish_sgrep -E '^[^ ]+')
    else
        # Posix systems print out the name of a signal using 'kill -l
        # SIGNUM', so we use this instead.
        complete -c kill -s l --description "List names of available signals"
        for i in (seq 31)
            set -g __kill_signals $__kill_signals $i" "(kill -l $i)
        end
    end
end
