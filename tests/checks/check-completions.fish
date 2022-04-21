#RUN: %fish -C 'set -l fish %fish' %s
# Test all completions where the command exists

# No output is good output
for f in $__fish_data_dir/completions/*.fish
    if type -q (string replace -r '.*/([^/]+).fish' '$1' $f)
        set -l out ($fish $f 2>&1 | string collect)
        test -n "$out"
        and echo -- OUTPUT from $f: $out
    end
end
