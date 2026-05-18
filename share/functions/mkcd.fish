# localization: tier1
#
# These are very common and useful
#

function mkcd --description "Create a directory and enter it"
    mkdir -p $argv
    and cd $argv
end
