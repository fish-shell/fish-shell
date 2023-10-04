#
# This allows us to use 'open FILENAME' to open a given file in the default
# application for the file.
#
if not command -sq open
    function open --description "Open file in default application"
        set -l options h/help
        argparse -n open $options -- $argv
        or return

        if set -q _flag_help
            __fish_print_help open
            return 0
        end

        if not set -q argv[1]
            printf (_ "%ls: Expected at least %d args, got only %d\n") open 1 0 >&2
            return 1
        end

        if type -q -f cygstart
            for i in $argv
                cygstart $i
            end
        else if type -q -f xdg-open
            for i in $argv
                xdg-open $i
            end
        else
            echo (_ 'No open utility found. Try installing "xdg-open" or "xdg-utils".') >&2
        end
    end
end
