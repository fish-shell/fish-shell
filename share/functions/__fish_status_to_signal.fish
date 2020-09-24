function __fish_status_to_signal --description "Print signal name from argument (\$status), or just argument"
    for arg in $argv
        if test $arg -gt 128
            set -l signals SIGHUP SIGINT SIGQUIT SIGILL SIGTRAP SIGABRT SIGBUS \
            SIGFPE SIGKILL SIGUSR1 SIGSEGV SIGUSR2 SIGPIPE SIGALRM \
            SIGTERM SIGSTKFLT SIGCHLD SIGCONT SIGSTOP SIGTSTP \
            SIGTTIN SIGTTOU SIGURG SIGXCPU SIGXFSZ SIGVTALRM \
            SIGPROF SIGWINCH SIGIO SIGPWR SIGSYS
            set -l sigix (math $arg - 128)
            if test $sigix -le (count $signals)
                echo $signals[$sigix]
            else
                echo $arg
            end
        else
            echo $arg
        end
    end
    return 0
end
