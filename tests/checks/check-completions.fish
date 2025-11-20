#RUN: fish=%fish %fish %s
# disable on CI ASAN because it's suuuper slow
#REQUIRES: test -z "$FISH_CI_SAN"
# Test all completions where the command exists

# No output is good output
for f in (status list-files completions | string match 'completions/*.fish')
    if type -q (string replace -r '.*/([^/]+).fish' '$1' $f)
        set -l out (__fish_data_with_file $f source 2>&1 | string collect)
        test -n "$out"
        and echo -- OUTPUT from $f: $out
    end
end
