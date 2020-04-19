function __fish_ps
    switch (realpath (command -v ps) | string match -r '[^/]+$')
        case busybox
            command ps $argv
        case '*'
            command ps axc $argv
    end
end
