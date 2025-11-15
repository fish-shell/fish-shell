# localization: skip(private)
function __fish_print_help --description "Print help for the specified fish function or builtin"
    set -l item (__fish_canonicalize_builtin $argv[1])

    function __fish_print_help_man -V item -a man1
        if not path is $man1
            # Trigger the "documentation not be installed" message. Currently
            # only when called from core.
            return 2
        end
        set -l args -l (path filter -- $man1/$item.1 $man1/$item.1.gz)[1]
        # Work around macOS/FreeBSD man not supporting -l yet (only mandoc really needs it).
        if not __fish_without_manpager command man -l $args[2] &>/dev/null
            set -e args[1]
        end
        command man $args
    end
    __fish_data_with_directory man/man1 \
        "$(string escape --style=regex $item.1)(?:\.gz)?" \
        __fish_print_help_man
    __fish_with_status functions --erase __fish_print_help_man
end
