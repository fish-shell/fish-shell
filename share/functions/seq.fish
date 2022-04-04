# If seq is not installed, then define a function that invokes __fish_fallback_seq
# We can't call type here because that also calls seq

if not command -sq seq
    if command -sq gseq
        # No seq provided by the OS, but GNU coreutils was apparently installed, fantastic
        function seq --description "Print sequences of numbers (gseq)"
            gseq $argv
        end
        exit
    else
        # No seq command
        function seq --description "Print sequences of numbers"
            __fish_fallback_seq $argv
        end
    end

    function __fish_fallback_seq --description "Fallback implementation of the seq command"
        set -l from 1
        set -l step 1
        set -l to 1

        # Remove a "--" argument if it happens first.
        if test "x$argv[1]" = x--
            set -e argv[1]
        end

        switch (count $argv)
            case 1
                set to $argv[1]
            case 2
                set from $argv[1]
                set to $argv[2]
            case 3
                set from $argv[1]
                set step $argv[2]
                set to $argv[3]
            case '*'
                printf (_ "%s: Expected 1, 2 or 3 arguments, got %d\n") seq (count $argv) >&2
                return 1
        end

        for i in $from $step $to
            if not string match -rq -- '^-?[0-9]*([0-9]*|\.[0-9]+)$' $i
                printf (_ "%s: '%s' is not a number\n") seq $i >&2
                return 1
            end
        end

        if test $step -ge 0
            set -l i $from
            while test $i -le $to
                echo $i
                set i (math -- $i + $step)
            end
        else
            set -l i $from
            while test $i -ge $to
                echo $i
                set i (math -- $i + $step)
            end
        end
    end
end
