function __fish_not_contain_opt -d "Checks that a specific option is not in the current command line"
    set -l next_short
    set -l short_opt
    set -l long_opt

    for i in $argv
        if test -n "$next_short"
            set next_short
            set short_opt $short_opt $i
        else
            switch $i
                case -s
                    set next_short 1
                case '-*'
                    echo __fish_not_contain_opt: Unknown option $i >&2
                    return 1

                case '*'
                    set long_opt $long_opt $i
            end
        end
    end

    for i in $short_opt
        if test -z "$i"
            continue
        end

        if string match -qr -- "^-$i|^-[^-]*$i" (commandline -cpx)
            return 1
        end

        if string match -qr -- "^-$i|^-[^-]*$i" (commandline -ct)
            return 1
        end
    end

    for i in $long_opt
        if test -z "$i"
            continue
        end

        if contains -- --$i (commandline -cpx)
            return 1
        end
    end

    return 0
end
