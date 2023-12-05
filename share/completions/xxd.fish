function __fish_seen_any_argument
    # Returns true if none of the switch names appear in the command line.
    for arg in $argv
        switch (string length $arg)
            case 2
                __fish_seen_argument -o $arg; and return 1
            case 1
                __fish_seen_argument -s $arg; and return 1
            case '*'
                __fish_seen_argument -l $arg; and return 1
        end
    end
end

set -l xxd_exclusive_args b e i ps r

complete -c xxd -s a -d 'Toggle autoskip'
complete -c xxd -s b -n "__fish_seen_any_argument $xxd_exclusive_args" -d 'Binary digit dump'
complete -c xxd -s C -n '__fish_seen_argument -s i' -d 'Capitalize variable names'
complete -c xxd -s c -xa '(seq 8 8 64)' -d 'Format COLS octets per line, default 16'
complete -c xxd -s E -d 'Show characters in EBCDIC'
complete -c xxd -s e -n "__fish_seen_any_argument $xxd_exclusive_args" -d 'Little-endian dump'
complete -c xxd -s g -xa '1 2 4' -d 'Octets per group in normal output, default 2'
complete -c xxd -s h -d 'Print help summary'
complete -c xxd -s i -n "__fish_seen_any_argument $xxd_exclusive_args" -d 'Output C-include file style'
complete -c xxd -s l -d 'Stop after NUM octets'
complete -c xxd -s o -x -d 'Add OFFSET to the displayed file position'
complete -c xxd -o ps -n "__fish_seen_any_argument $xxd_exclusive_args" -d 'Output PostScript/plain hexdump style'
complete -c xxd -s r -d 'Reverse operation: convert hexdump into binary'
complete -c xxd -s s -xa '-32 -8 +8 +32' -n 'not __fish_seen_argument -s r' -d 'Start at SEEK bytes offset in file'
complete -c xxd -s s -xa '-32 -8 +8 +32' -n '__fish_seen_argument -s r' -d 'Add OFFSET to file positions in hexdump'
complete -c xxd -s d -d 'Show offset in decimal, not hex'
complete -c xxd -s u -d 'Use uppercase hex letters'
complete -c xxd -s v -d 'Show version'
