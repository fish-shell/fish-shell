function __fish_argcomplete_complete
    set -lx _ARGCOMPLETE 1
    set -lx _ARGCOMPLETE_IFS \n
    set -lx _ARGCOMPLETE_SUPPRESS_SPACE 1
    set -lx _ARGCOMPLETE_SHELL fish
    set -lx COMP_LINE (commandline -pc)
    set -lx COMP_POINT (string length (commandline -cp))
    set -lx COMP_TYPE
    $argv 8>&1 9>&2 2>/dev/null
end
