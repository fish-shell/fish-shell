#RUN: %fish %s

{

false
export FOO=bar
echo $status
# CHECK: 0
set -S FOO
# CHECK: $FOO: set in global scope, exported, with 1 elements
# CHECK: $FOO[1]: |bar|

## Test path variable that is not already set.
set -eg MANPATH
export MANPATH=":/path/to/man/pages"
echo $status
# CHECK: 0
set -S MANPATH
# CHECK: $MANPATH: set in global scope, exported, a path variable with 2 elements
# CHECK: $MANPATH[1]: ||
# CHECK: $MANPATH[2]: |/path/to/man/pages|

## Re-set a path variable that is already set.
export MANPATH="/some/path:/some/other/path"
echo $status
# CHECK: 0
set -S MANPATH
# CHECK: $MANPATH: set in global scope, exported, a path variable with 2 elements
# CHECK: $MANPATH[1]: |/some/path|
# CHECK: $MANPATH[2]: |/some/other/path|

## Test that it's exported properly.
env | string match 'MANPATH=*'
# CHECK: MANPATH=/some/path:/some/other/path

} | string match -v '*originally inherited*'
