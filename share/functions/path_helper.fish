# Reimplement the macOS /usr/libexec/path_helper executable for fish;
# see __fish_construct_path in config.fish and
# https://opensource.apple.com/source/shell_cmds/shell_cmds-203/path_helper/path_helper.c.auto.html .

function __quote -d "single-quote arguments, escaping characters as necessary"
    for x in $argv
        set -l quoted (string replace -a -r '([\\\\\'])' '\\\\\\\\$1' $x)
        echo "'$quoted'"
    end
end

function path_helper -d "helper for constructing PATH environment variable"
    set -l path (__fish_construct_path "PATH" "/etc/paths" "/etc/paths.d")
    set -l quoted_path (__quote $path)
    echo "set -xg PATH $quoted_path"

    if [ -n "$MANPATH" ]
        set -l manpath (__fish_construct_path "MANPATH" "/etc/manpaths" "/etc/manpaths.d")
        set -l quoted_manpath (__quote $manpath)
        echo "set -xg MANPATH $quoted_manpath"
    end
end
