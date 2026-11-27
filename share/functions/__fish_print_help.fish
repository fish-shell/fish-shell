# localization: skip(private)
function __fish_print_help --description "Print help for the specified fish function or builtin"
    if not type -q man
        fish_command_not_found man
        return 1
    end

    set -l item (__fish_canonicalize_builtin $argv[1])

    function __fish_print_help_man -V item -a man1
        set -l args -l $(path filter -- "$man1/$item.1" "$man1/$item.1.gz")[1]
        if not set -q args[2]
            # Builtins recognize status 2 as a missing manual page.
            return 2
        end
        # Work around macOS/FreeBSD man not supporting -l yet (only mandoc really needs it).
        if not __fish_without_manpager man -l $args[2] &>/dev/null
            set -e args[1]
        end
        man $args
    end
    __fish_data_with_directory man/man1 \
        "$(string escape --style=regex $item.1)(?:\.gz)?" \
        __fish_print_help_man
    __fish_with_status functions --erase __fish_print_help_man
end
