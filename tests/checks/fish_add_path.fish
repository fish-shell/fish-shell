# RUN: %fish %s
#
# This deals with $PATH manipulation. We need to be careful not to step on anything.

set -l tmpdir (mktemp -d)
mkdir $tmpdir/bin
mkdir $tmpdir/sbin
ln -s $tmpdir/bin $tmpdir/link

# We set fish_user_paths to an empty global to have a starting point
set -g fish_user_paths
fish_add_path -v $tmpdir/bin
# CHECK: set fish_user_paths {{.*}}/bin

# Not adding duplicates, so this adds nothing, so it's a failing status
fish_add_path -v $tmpdir/bin
echo $status
# CHECK: 1

# Not adding a link either
fish_add_path -v $tmpdir/link
echo $status
# CHECK: 1

fish_add_path -a $tmpdir/sbin
# Not printing anything because it's not verbose, the /sbin should be added at the end.
string replace -- $tmpdir '' $fish_user_paths | string join ' '
# CHECK: /bin /sbin

fish_add_path -m $tmpdir/sbin
string replace -- $tmpdir '' $fish_user_paths | string join ' '
# CHECK: /sbin /bin

set -l oldpath "$PATH"
fish_add_path -nP $tmpdir/etc | string replace -- $tmpdir ''
# Should print a set command to prepend /etc to $PATH, but not actually do it
# CHECK: set PATH /etc{{.*}}

# Confirm that $PATH didn't change.
test "$oldpath" = "$PATH"
or echo "PATH CHANGED!!!" >&2

exit 0
