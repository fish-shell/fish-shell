function __ghoti_ps
    switch (builtin realpath (command -v ps) | string match -r '[^/]+$')
        case busybox
            command ps $argv
        case '*'
            command ps axc $argv
    end
end
