# This function is intended to be used to generate completions in `-a "(...)"` calls for arguments that take 1..n values
# separated by commas and we dynamically or statically generated a list of n values that must be combined and permuted
# in order to appease both the `complete` machinery and the target application.
#
# TODO: Make this work with completions that have their description appended after a \t

function __fish_concat_completions -d "Generate completions that are specified as comma-separated values from stdin source"
    type -q awk || return

    set -l token (commandline -ct)
    # We want to filter out suggestions the user has already entered.
    set -l extant (string split -- ',' $token)
    set -l filter (printf '^%s$\n' (printf '%s\n' $extant | string escape --style=regex) | string join '|')
    # Work around the insanity of trying to read from stdin within a function. Note that we can't place the
    # `read` call in between () to capture the output because that breaks its connection to stdin.
    while read -l line
        echo $line
    end | string match -er '.' | string match -rv -- $filter | string replace -r '^' -- (string replace -rf -- '^(.+),.*$' '$1,' $token; or echo "")

    return
    # Verified compatible with bsd awk and gnu awk
    awk '
        {
            lines[NR-1] = $0
        }
        END {
            n = NR
            for (k = 1; k <= n; k++) {
                generate_permutations(lines, n, k)
            }
        }
        function generate_permutations(arr, total_len, perm_len, prefix, _i, _swap) {
            if (perm_len == 0) {
                print prefix
                return
            }
            for (_i = 0; _i < total_len; _i++) {
                _swap = arr[total_len-1]; arr[total_len-1] = arr[_i]; arr[_i] = _swap
                generate_permutations(arr, total_len-1, perm_len-1, prefix arr[total_len-1] (perm_len == 1 ? "" : ","))
                arr[_i] = arr[total_len-1]; arr[total_len-1] = _swap
            }
        }
    '
end
