#
# Test to see if we've seen a subcommand from a list.
# This logic may seem backwards, but the commandline will often be much shorter
# than the list
#

function __fish_seen_subcommand_from
    set -l regex (string escape --style=regex -- (commandline -pxc)[2..] | string join '|')
    string match -rq -- "^$regex"'$' $argv
end
