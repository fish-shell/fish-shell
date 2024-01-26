function __fish_anyeditor --description "Print a editor to use, or an error message"
    set -l editor
    if set -q VISUAL
        echo $VISUAL | read -at editor
    else if set -q EDITOR
        echo $EDITOR | read -at editor
    else
        echo >&2
        echo >&2 (_ 'External editor requested but $VISUAL or $EDITOR not set.')
        echo >&2 (_ 'Please set VISUAL or EDITOR to your preferred editor.')
        commandline -f repaint
        return 1
    end
    string join \n $editor
    return 0
end
