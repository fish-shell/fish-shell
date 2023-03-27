#
# Find directories that complete $argv[1], output them as completions
# with description $argv[2] if defined, otherwise use 'Directory'.
# If no arguments are provided, attempts to complete current commandline token.
#
function __fish_complete_directories -d "Complete directory prefixes" --argument-names comp desc
    if not set -q desc[1]
        set desc Directory
    end

    if not set -q comp[1]
        # No token given, so we use the current commandline token.
        # If we have a --name=val option, we need to remove it,
        # or the complete -C below would keep it, and then whatever complete
        # called us would add it again (assuming it's in the current token)
        set comp (commandline -ct | string replace -r -- '^-[^=]*=' '' $comp)
    end

    # HACK: We call into the file completions by using an empty command
    # If we used e.g. `ls`, we'd run the risk of completing its options or another kind of argument.
    # But since we default to file completions, if something doesn't have another completion...
    # (really this should have an actual complete option)
    set -l dirs (complete -C"'' $comp" | string match -r '.*/$')

    if set -q dirs[1]
        printf "%s\n" $dirs\t"$desc"
    end
end
