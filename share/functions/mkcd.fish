# localization: tier1
#
# These are very common and useful
#

function mkcd --description "Create a directory and enter it"
    if test (count $argv) -ne 1
        echo "mkcd: expected 1 argument, got "(count $argv) >&2
        return 1
    end
    mkdir -p $argv
    and cd $argv
end
