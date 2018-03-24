# Reimplement the macOS /usr/libexec/path_helper executable for fish; see
# https://opensource.apple.com/source/shell_cmds/shell_cmds-203/path_helper/path_helper.c.auto.html .

function __construct_path -d "construct a path variable"
    set -l result

    for path_file in $argv[2] $argv[3]/*
        if test -f $path_file
            while read -la entry
                if not contains $entry $result
                    set result $result $entry
                end
            end <$path_file
        end
    end

    for entry in $$argv[1]
        if not contains $entry $result
            set result $result $entry
        end
    end

    for x in $result
        echo $x
    end
end

function __quote -d "single-quote arguments, escaping characters as necessary"
    for x in $argv
        set -l quoted (string replace -a -r '([\\\\\'])' '\\\\\\\\$1' $x)
        echo "'$quoted'"
    end
end

function path_helper -d "helper for constructing PATH environment variable"
    set -l path (__construct_path "PATH" "/etc/paths" "/etc/paths.d")
    set -l quoted_path (__quote $path)
    echo "set -xg PATH $quoted_path"

    if [ $MANPATH ]
        set -l manpath (__construct_path "MANPATH" "/etc/manpaths" "/etc/manpaths.d")
        set -l quoted_manpath (__quote $manpath)
        echo "set -xg MANPATH $quoted_manpath"
    end
end
