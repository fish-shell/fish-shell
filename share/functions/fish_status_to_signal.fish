function fish_status_to_signal --description "Print signal name from argument (\$status), or just argument"
    for arg in $argv
        if test $arg -gt 128
            set -l sigix (math $arg-128)
            if test $sigix -le 31 # 31 = number of named signals below
                set -l signals HUP INT QUIT ILL TRAP ABRT BUS FPE KILL USR1 \
                    SEGV USR2 PIPE ALRM TERM STKFLT CHLD CONT STOP TSTP \
                    TTIN TTOU URG XCPU XFSZ VTALRM PROF WINCH IO PWR SYS
                echo SIG$signals[$sigix]
            else
                echo $arg
            end
        else
            echo $arg
        end
    end
    return 0
end
