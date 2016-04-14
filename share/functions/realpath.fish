# Provide a minimalist realpath implementation to help deal with platforms that may not provide it
# as a command. If a realpath command is available simply pass all arguments thru to it. If not
# fallback to alternative solutions.
function realpath --description 'realpath implementation with fallbacks if command not present'
    if type -q -P realpath
        command realpath $argv
    else if type -q -P readlink
        readlink -f $argv[-1]
    else if type -q -P python2
        python2 -c 'import os, sys; print os.path.realpath(sys.argv[1])' $argv[-1]
    else if type -q -P python3
        python3 -c 'import os, sys; print(os.path.realpath(sys.argv[1]))' $argv[-1]
    else
        echo 'error: cannot find a way to implement realpath' >&2
        echo $argv[-1]
        exit 1
    end
end
