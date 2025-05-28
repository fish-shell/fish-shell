#RUN: %fish %s

function foo; end
mkdir $__fish_config_dir/completions
echo >$__fish_config_dir/completions/foo.fish "echo auto-loading foo.fish"
complete -C "foo " >/dev/null
# CHECK: auto-loading foo.fish
complete -C "foo " >/dev/null
# already loaded
