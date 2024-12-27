#RUN: fish=%fish %fish %s
# disable on CI ASAN because it's suuuper slow
#REQUIRES: test -z "$FISH_CI_SAN"
# Test all completions where the command exists

# No output is good output
for f in $__fish_data_dir/completions/*.fish
    if type -q (string replace -r '.*/([^/]+).fish' '$1' $f)
        set -l out (source $f 2>&1 | string collect)
        test -n "$out"
        and echo -- OUTPUT from $f: $out
    end
end
