#RUN: %ghoti -C 'set -l ghoti %ghoti' %s
# Test all completions where the command exists

# No output is good output
for f in $__ghoti_data_dir/completions/*.ghoti
    if type -q (string replace -r '.*/([^/]+).ghoti' '$1' $f)
        set -l out (source $f 2>&1 | string collect)
        test -n "$out"
        and echo -- OUTPUT from $f: $out
    end
end
