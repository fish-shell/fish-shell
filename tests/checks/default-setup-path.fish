#RUN: fish=%fish %fish %s

if command -q getconf
    # (no env -u, some systems don't support that)
    set -l getconf (command -s getconf)
    set -e PATH
    $fish -c 'test "$PATH" = "$('"$getconf"' PATH)"; and echo Success'
else
    # this is DEFAULT_PATH
    # the first element (usually `/usr/local/bin`) depends on PREFIX set in CMake, so we ignore it
    set -e PATH
    $fish -c 'test "$PATH[2..]" = "/usr/bin:/bin"; and echo Success'
end
# CHECK: Success

# Note: $PATH is now busted, I suggest abandoning this file.
