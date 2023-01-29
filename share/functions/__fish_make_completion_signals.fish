function __fish_make_completion_signals --description 'Make list of kill signals for completion'
    set -q __kill_signals
    and return 0

    set -g __kill_signals

    # Cygwin's kill is special, and the documentation lies.
    # Just hardcode the signals.
    set -l os (uname)
    if string match -q 'CYGWIN*' -- $os
        or string match -eiq Msys -- $os
        or string match -eiq mingw -- $os
        set -a __kill_signals "1 HUP" "2 INT" "3 QUIT" "4 ILL" "5 TRAP" "6 ABRT" \
            "6 IOT" "7 BUS" "8 FPE" "9 KILL" "10 USR1" "11 SEGV" \
            "12 USR2" "13 PIPE" "14 ALRM" "15 TERM" "16 STKFLT" "17 CHLD" \
            "17 CLD" "18 CONT" "19 STOP" "20 TSTP" "21 TTIN" "22 TTOU" \
            "23 URG" "24 XCPU" "25 XFSZ" "26 VTALRM" "27 PROF" "28 WINC" \
            "29 IO" "29 POLL" "30 PWR" "31 SYS" "34 RTMIN" "64 RTMA"
        return
    end

    # If we don't have a kill, we don't try to run it.
    # We require a kill *command* because we used to unconditionally define
    # a wrapper (to allow %foo expansion)
    command -q kill
    or return

    # Some systems use the GNU coreutils kill command where `kill -L` produces an extended table
    # format that looks like this:
    #
    #  1 HUP    Hangup: 1
    #  2 INT    Interrupt: 2
    #
    # The procps `kill -L` produces a more compact table. We can distinguish the two cases by
    # testing whether it supports `kill -t`; in which case it is the coreutils `kill` command.
    # Darwin doesn't have kill -t or kill -L
    if kill -t 2>/dev/null >/dev/null
        or not kill -L 2>/dev/null >/dev/null
        # Posix systems print out the name of a signal using 'kill -l SIGNUM'.
        complete -c kill -s l --description "List names of available signals"
        for i in (seq 31)
            set -a __kill_signals $i" "(kill -l $i 2>/dev/null | string upper)
        end
    else
        # util-linux (on Arch) and procps-ng (on Debian) kill use 'kill -L' to write out a numbered list
        # of signals. Use this to complete on both number _and_ on signal name.
        complete -c kill -s L --description "List codes and names of available signals"
        kill -L 2>/dev/null | string trim | string replace -ra '   *' \n | while read -l signo signame
            set -a __kill_signals "$signo $signame"
        end
    end
end
