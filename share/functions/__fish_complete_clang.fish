# This function is compatible with clang, clang++, and variations thereof, and is shared
# by clang.fish and clang++.fish.
# We dynamically query the head at `(commandline -x)[1]` to get the correct completions.
function __fish_complete_clang
    # If the result is for a value, clang only prints the value, so completions
    # for `-std=` print `c++11` and not `-std=c++11` like we need. See #4174.
    set -l prefix (commandline -ct | string replace -fr -- '^(.*=)[^=]*' '$1')

    # Don't hard-code the name of the clang binary
    set -l clang (commandline -xp)[1]
    # first get the completions from clang, with the prefix separated from the value by a comma
    $clang --autocomplete=(commandline -ct | string unescape | string replace -- "$prefix" "$prefix,") 2>/dev/null |
        # and put it in a format that fish understands
        string replace -r -- '^([^ ]+)\s*(.*)' "$prefix\$1\t\$2"
end
