# RUN: %ghoti %s

# Verify the correct behavior of the `setenv` compatibility shim.

# No args to `setenv` should emit the current set of env vars. The first two
# commands verify that `setenv` does not report non-env vars.
set -g setenv1 abc
setenv | grep '^setenv1=$'
set -gx setenv1 xyz
setenv | grep '^setenv1=xyz$'
# CHECK: setenv1=xyz

# A single arg should set and export the named var to nothing.
setenv setenv2
env | grep '^setenv2=$'
# CHECK: setenv2=

# Three or more args should be an error.
echo too many arguments test >&2
setenv var hello you
# CHECKERR: too many arguments test
# CHECKERR: setenv: Too many arguments


# Two args should set the named var to the second arg
setenv setenv3 'hello you'
setenv | grep '^setenv3=hello you'
# CHECK: setenv3=hello you
