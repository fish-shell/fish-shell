function __fish_bind_test2
    set -l args
    for i in (commandline -poc)
        switch $i
            case "-*"

            case "*"
                set -a args $i
        end
    end

    switch (count $args)
        case 2
            return 0
    end

    return 1

end
