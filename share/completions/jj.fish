# The reason for `__this-command-does-not-exist` is that, if dynamic completion
# is not implemented, we'd like to get an error reliably. However, the
# behavior of `jj` without arguments depends on the value of a config, see
# https://jj-vcs.github.io/jj/latest/config/#default-command
if set -l completion (COMPLETE=fish jj __this-command-does-not-exist 2>/dev/null)
    # jj is new enough for dynamic completions to be implemented
    printf %s\n $completion | source
else
    jj util completion fish | source
end
