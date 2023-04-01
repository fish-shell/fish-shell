function __ghoti_contains_opt -d "Checks if a specific option has been given in the current commandline"
    set -l next_short
    set -l short_opt
    set -l long_opt

    for i in $argv
        if test -n "$next_short"
            set next_short
            set -a short_opt $i
        else
            switch $i
                case -s
                    set next_short 1
                case '-*'
                    echo __ghoti_contains_opt: Unknown option $i >&2
                    return 1
                case '*'
                    set -a long_opt $i
            end
        end
    end

    for i in $short_opt
        if test -z "$i"
            continue
        end

        if string match -qr -- "^-$i|^-[^-]*$i" (commandline -cpo)
            return 0
        end

        if string match -qr -- "^-$i|^-[^-]*$i" (commandline -ct)
            return 0
        end
    end

    for i in $long_opt
        if test -z "$i"
            continue
        end

        if contains -- --$i (commandline -cpo)
            return 0
        end
    end

    return 1
end
