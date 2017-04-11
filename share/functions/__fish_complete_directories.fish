#
# Find directories that complete $argv[1], output them as completions
# with description $argv[2] if defined, otherwise use 'Directory'
#
function __fish_complete_directories -d "Complete directory prefixes" --argument comp desc
    if not set -q desc[1]
        set desc (_ Directory)
    end

    set -l dirs $comp*/
    if set -q dirs[1]
        printf "%s\t$desc\n" $dirs
    end
end
