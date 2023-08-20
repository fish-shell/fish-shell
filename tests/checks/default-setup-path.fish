#RUN: %fish -C 'set -g fish %fish' %s

# FIXME: Rationalize behavior when PATH is explicitly unset, should this not behave like PATH=""?
# "" is threated like ".", see https://github.com/fish-shell/fish-shell/issues/3914
if command -q getconf
    env -u PATH $fish -c 'test "$PATH" = "$('(command -s getconf)' PATH)"; and echo Success'
else
    # this is DEFAULT_PATH
    # the first element (usually `/usr/local/bin`) depends on PREFIX set in CMake, so we ignore it
    env -u PATH $fish -c 'test "$PATH[2..]" = "/usr/bin:/bin"; and echo Success'
end
# CHECK: Success
