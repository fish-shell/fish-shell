# check if command fish_key_reader works and is the same version that
# came with this fish. This will happen one time.
command -sq fish_key_reader
and command fish_key_reader --version 2>&1 | string match -q -- $version
# if alias doesn't define the function here, this is an autoloaded "nothing".
# the command (if there is one) will be used by default.
or alias fish_key_reader=(string escape $__fish_bin_dir/fish_key_reader)
