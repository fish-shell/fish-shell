#
# This is a neat function, stolen from zsh. It allows you to edit the
# value of a variable interactively.
#

function vared --description "Edit variable value"
    if test (count $argv) = 1
        switch $argv

            case -h --h --he --hel --help
                __fish_print_help vared
                return 0

            case '-*'
                printf (_ "%s: Unknown option %s\n") vared $argv
                return 1

            case '*'
                if test (count $$argv ) -lt 2
                    # Avoid using any local variables in this function, otherwise they can't be edited
                    # https://github.com/fish-shell/fish-shell/issues/8836

                    # The command substitution in this line controls the scope.
                    # If variable already exists, do not add any switches, so we don't change
                    # scoping or export rules. But if it does not exist, we make the variable
                    # global, so that it will not die when this function dies.

                    read -p 'set_color green; echo '$argv'; set_color normal; echo "> "' \
                        (if not set -q $argv; echo -g; end) \
                        -c "$$argv" \
                        $argv
                else
                    printf (_ '%s: %s is an array variable. Use %svared%s %s[n]%s to edit the n:th element of %s\n') vared $argv (set_color $fish_color_command; echo) (set_color $fish_color_normal; echo) $argv (set_color normal; echo) $argv
                end
        end
    else
        printf (_ '%s: Expected exactly one argument, got %s.\n\nSynopsis:\n\t%svared%s VARIABLE\n') vared (count $argv) (set_color $fish_color_command; echo) (set_color $fish_color_normal; echo)
    end
end
