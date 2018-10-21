#
# Find directories that complete $argv[1], output them as completions
# with description $argv[2] if defined, otherwise use 'Directory'.
# If no arguments are provided, attempts to complete current commandline token.
#
function __fish_complete_directories -d "Complete directory prefixes" --argument comp desc
    if not set -q desc[1]
        set desc (_ "Directory")
    end

    if not set -q comp[1]
        set comp (commandline -ct)
    end

    # Inherit support for completing expansions from __fish_complete_suffix
    # Supports completing expressions like ~/foo or x$bar, etc.
    set -l dirs (__fish_complete_suffix $comp /)

    if set -q dirs[1]
        printf "%s\t$desc\n" $dirs
    end
end
