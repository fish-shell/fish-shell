# localization: skip(private)
function __fish_uname
    if not set -q __fish_uname
        set -g __fish_uname (uname)
    end
    echo -- $__fish_uname
end
