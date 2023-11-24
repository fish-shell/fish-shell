#RUN: %fish -C 'set -g fish %fish' %s

if command -q getconf
    env -u PATH $fish -c 'test "$PATH" = "$('(command -s getconf)' PATH)"; and echo Success'
else
    # this is DEFAULT_PATH
    # the first element (usually `/usr/local/bin`) depends on PREFIX set in CMake, so we ignore it
    env -u PATH $fish -c 'test "$PATH[2..]" = "/usr/bin:/bin"; and echo Success'
end
# CHECK: Success
