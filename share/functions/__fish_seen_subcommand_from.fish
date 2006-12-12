#
# Test to see if we've seen a subcommand from a list.
# This logic may seem backwards, but the commandline will often be much shorter
#  than the list
#

function __fish_seen_subcommand_from
    set -l cmd (commandline -poc)
    set -e cmd[1]
    for i in $cmd
        if contains -- $i $argv
            return 0
        end
    end
    return 1
end
