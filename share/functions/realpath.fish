# Provide a minimalist realpath implementation to help deal with platforms that may not provide it
# as a command. If a realpath command is available simply pass all arguments thru to it. If not
# fallback to alternative solutions.

# The following is slightly subtle. The first time `realpath` is invoked this script will be read.
# If we see that there is an external command by that name we just return. That will cause fish to
# run the external command. On the other hand, if an external command isn't found we define a
# function that will provide fallback behavior.
if not type -q -P realpath
    function realpath --description 'fallback realpath implementation'
        builtin fish_realpath $argv[-1]
    end
end
