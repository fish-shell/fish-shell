# localization: skip(private)
function __fish_without_manpager
    MANPAGER=cat WHATISPAGER=cat if test "$argv[1]" = command
        command $argv[2..]
    else
        $argv
    end
end
