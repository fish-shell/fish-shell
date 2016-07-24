function __fish_make_completion_signals --description 'Make list of kill signals for completion'
    set -q __kill_signals
    and return 0

    # Some systems use the GNU coreutils kill command where `kill -L` produces an extended table
    # format that looks like this:
    #
    #  1 HUP    Hangup: 1
    #  2 INT    Interrupt: 2
    #
    # The procps `kill -L` produces a more compact table. We can distinguish the two cases by
    # testing whether it supports `kill -t`; in which case it is the coreutils `kill` command.
    # Darwin doesn't have kill -t or kill -L
    if kill -t ^/dev/null >/dev/null; or not kill -L ^/dev/null >/dev/null
        # Posix systems print out the name of a signal using 'kill -l SIGNUM'.
        complete -c kill -s l --description "List names of available signals"
        for i in (seq 31)
            set -g __kill_signals $__kill_signals $i" "(kill -l $i | tr '[:lower:]' '[:upper:]')
        end
    else
        # Debian and some related systems use 'kill -L' to write out a numbered list
        # of signals. Use this to complete on both number _and_ on signal name.
        complete -c kill -s L --description "List codes and names of available signals"
        set -g __kill_signals
        kill -L | sed -e 's/^ //; s/  */ /g; y/ /\n/' | while read -l signo
            test -z "$signo"
            and break  # the sed above produces one blank line at the end
            read -l signame
            set -g __kill_signals $__kill_signals "$signo $signame"
        end
    end
end
