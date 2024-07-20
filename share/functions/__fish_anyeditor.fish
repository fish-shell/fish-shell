function __fish_anyeditor --description "Print a editor to use, or an error message"
    set -l editor
    if set -q VISUAL
        echo $VISUAL | read -at editor
    else if set -q EDITOR
        echo $EDITOR | read -at editor
    else
        __fish_echo string join \n -- \
            (_ 'External editor requested but $VISUAL or $EDITOR not set.') \
            (_ 'Please set VISUAL or EDITOR to your preferred editor.')
        return 1
    end
    string join \n -- $editor
    return 0
end
