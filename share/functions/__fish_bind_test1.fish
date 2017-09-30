function __fish_bind_test1
    set -l args
    set -l use_keys no
    for i in (commandline -poc)
        switch $i
            case -k --k --ke --key
                set use_keys yes

            case "-*"

            case "*"
                set -a args $i
        end
    end

    switch $use_keys
        case yes
            switch (count $args)
                case 1
                    return 0
            end
    end
    return 1
end
