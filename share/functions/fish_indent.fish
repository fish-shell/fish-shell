# check if command fish_indent works and is the same version that
# came with this fish. This will happen one time.
command -sq fish_indent
and command fish_indent --version 2>&1 | string match -rq $version
# if we don't define the function here, this is an autoloaded "nothing".
# the command (if there is one) will be used by default.
or function fish_indent
    $__fish_bin_dir/fish_indent $argv
end
