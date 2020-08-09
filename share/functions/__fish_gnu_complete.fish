function __fish_gnu_complete -d "Wrapper for the complete built-in. Skips the long completions on non-GNU systems"
    set -l is_gnu 0

    set -l argv_out

    # Check if we are using a gnu system
    for i in $argv
        switch $i

            case -g --is-gnu
                set is_gnu 1

            case '*'
                set argv_out $argv_out $i
        end
    end

    set argv $argv_out
    set argv_out
    set -l skip_next 0

    # Remove long option if not on a gnu system
    switch $is_gnu
        case 0
            for i in $argv

                switch $skip_next

                    case 1
                        set skip_next 0
                        continue

                end

                switch $i

                    case -l --long
                        set skip_next 1
                        continue

                end

                set argv_out $argv_out $i
            end
            set argv $argv_out

    end

    complete $argv

end
