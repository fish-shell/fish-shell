function __fish_ps
    switch (command -s ps | path resolve | path basename)
        case busybox
            command ps $argv
        case '*'
            command ps axc $argv
    end
end
