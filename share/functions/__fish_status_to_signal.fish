function __fish_status_to_signal --description "Print signal name from argument (\$status), or just argument"
    if test (count $argv) -ne 1
        echo "expected single argument as integer from \$status" >&2
        return 1
    end

    if test $argv[1] -gt 128
        set -l signals SIGHUP SIGINT SIGQUIT SIGILL SIGTRAP SIGABRT SIGBUS \
            SIGFPE SIGKILL SIGUSR1 SIGSEGV SIGUSR2 SIGPIPE SIGALRM \
            SIGTERM SIGSTKFLT SIGCHLD SIGCONT SIGSTOP SIGTSTP \
            SIGTTIN SIGTTOU SIGURG SIGXCPU SIGXFSZ SIGVTALRM \
            SIGPROF SIGWINCH SIGIO SIGPWR SIGSYS
        set -l sigix (math $argv[1] - 128)
        if test $sigix -le (count $signals)
            echo $signals[$sigix]
            return 0
        end
    end

    echo $argv[1]
    return 0
end
