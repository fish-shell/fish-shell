# RUN: %fish %s
#
# This deals with $PATH manipulation. We need to be careful not to step on anything.

set -l tmpdir (mktemp -d)
mkdir $tmpdir/bin
mkdir $tmpdir/sbin
mkdir $tmpdir/etc
ln -s $tmpdir/bin $tmpdir/link

# We set fish_user_paths to an empty global to have a starting point
set -g fish_user_paths
fish_add_path -v $tmpdir/bin
# CHECK: set fish_user_paths {{.*}}/bin
echo $status
# CHECK: 0

# Confirm that it actually ends up in $PATH
contains -- (builtin realpath $tmpdir/bin) $PATH
and echo Have bin
# CHECK: Have bin

# Not adding duplicates and not triggering variable handlers
function checkpath --on-variable PATH --on-variable fish_user_paths; echo CHECKPATH: $argv; end
set PATH $PATH
# CHECK: CHECKPATH: VARIABLE SET PATH
fish_add_path -v $tmpdir/bin
# CHECK: Skipping already included path: {{.*}}
# CHECK: No paths to add, not setting anything.
# Nothing happened, so the status failed.
echo $status
# CHECK: 1
functions --erase checkpath

# Add a link to the same path.
fish_add_path -v $tmpdir/link
# CHECK: set fish_user_paths {{.*}}/link {{.*}}/bin
echo $status
# CHECK: 0

# Relative paths are made absolute
set -l oldpwd $PWD
cd $tmpdir
fish_add_path -nv sbin .
# CHECK: set fish_user_paths /{{.*}}/sbin /{{.*}} /{{.*}}/link /{{.*}}/bin
cd $oldpwd

fish_add_path -a $tmpdir/sbin
# Not printing anything because it's not verbose, the /sbin should be added at the end.
string replace -- $tmpdir '' $fish_user_paths | string join ' '
# CHECK: /link /bin /sbin

fish_add_path -m $tmpdir/sbin
string replace -- $tmpdir '' $fish_user_paths | string join ' '
# CHECK: /sbin /link /bin

set -l oldpath "$PATH"
fish_add_path -nP $tmpdir/etc | string replace -- $tmpdir ''
# Should print a set command to prepend /etc to $PATH, but not actually do it
# CHECK: set -g PATH /etc{{.*}}

# Confirm that $PATH didn't change.
test "$oldpath" = "$PATH"
or echo "PATH CHANGED!!!" >&2

# See that moving multiple arguments removes the correct ones - #7776
PATH=$tmpdir/{bin,etc,link,sbin} fish_add_path -nPpm $tmpdir/{link,sbin} | string replace -a $tmpdir ''
# CHECK: set -g PATH /link /sbin /bin /etc

# See that trying to add a path twice doesn't duplicate it
PATH=$tmpdir/{bin,etc,link,sbin} fish_add_path -nPpm $tmpdir/sbin{,} | string replace -a $tmpdir ''
# CHECK: set -g PATH /sbin /bin /etc /link
exit 0
