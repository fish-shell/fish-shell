#RUN: fish=%fish %fish %s

# REQUIRES: %fish -c "is_cygwin"

mkdir dir
echo "#!/bin/sh" >dir/foo.exe
echo "#!/bin/sh" >dir/foo.bar
set PATH (pwd)/dir $PATH

# === Check that `complete` prefers to non-exe name, unless the user started
# to type the extension
complete -C"./dir/fo"
# CHECK: ./dir/foo{{\t}}command
# CHECK: ./dir/foo.bar{{\t}}command
complete -C"./dir/foo."
# CHECK: ./dir/foo.bar{{\t}}command
# CHECK: ./dir/foo.exe{{\t}}command

# === Check that foo.exe uses foo's description if it doesn't have its own
function __fish_describe_command
    echo -e "foo\tposix"
end
complete -C"./dir/foo."
# CHECK: ./dir/foo.bar{{\t}}command
# CHECK: ./dir/foo.exe{{\t}}Posix
function __fish_describe_command
    echo -e "foo\tposix"
    echo -e "foo.exe\twindows"
end
complete -C"./dir/fo"
# CHECK: ./dir/foo{{\t}}Posix
# CHECK: ./dir/foo.bar{{\t}}command
complete -C"./dir/foo."
# CHECK: ./dir/foo.bar{{\t}}command
# CHECK: ./dir/foo.exe{{\t}}Windows

# === Check that if we have a non-exe and an exe file, they both show
echo "#!/bin/sh" >dir/foo.bar.exe
complete -C"./dir/foo.ba"
# CHECK: ./dir/foo.bar{{\t}}command
# CHECK: ./dir/foo.bar.exe{{\t}}command

# === Check that "foo.fish" completion file is used when completing "foo.exe"
# and there is no "foo.exe.fish"
mkdir $__fish_config_dir/completions
echo "complete -c foo -s a -d args; complete -c foo -s b -d bargs" >$__fish_config_dir/completions/foo.fish
complete -C"./dir/foo.exe -"
# CHECK: -a{{\t}}args
# CHECK: -b{{\t}}bargs

# === Check that "foo.exe.fish" is used over "foo.fish" when both are present
# when completing "foo.exe" (but still uses "foo.fish" for "foo")
# Note: use subshell to avoid waiting 15s for the autoload cache to become stale
echo "complete -c foo -s c -d cargs; complete -c foo -s d -d dargs" >$__fish_config_dir/completions/foo.exe.fish
$fish -ic 'complete -C"./dir/foo.exe -"'
# CHECK: -c{{\t}}cargs
# CHECK: -d{{\t}}dargs
$fish -ic 'complete -C"./dir/foo -"'
# CHECK: -a{{\t}}args
# CHECK: -b{{\t}}bargs

# === We only support exe=>non-exe fallback for description/args completion.
# We do not handle the other way around
function __fish_describe_command
    echo -e "foo.bar.exe\twindows"
end
rm $__fish_config_dir/completions/foo.fish
complete -C"./dir/foo.ba"
# CHECK: ./dir/foo.bar{{\t}}command
# CHECK: ./dir/foo.bar.exe{{\t}}Windows
$fish -ic 'complete -C"./dir/foo -"'
# nothing
