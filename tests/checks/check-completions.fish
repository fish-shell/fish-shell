#RUN: %fish -C 'set -l fish %fish' %s
# Test all completions where the command exists

# No output is good output
for f in $__fish_data_dir/completions/*.fish
    if command -q (string replace -r '.*/([^/]+).fish' '$1' $f)
        $fish $f
    end
end
