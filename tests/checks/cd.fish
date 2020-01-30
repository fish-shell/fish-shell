# RUN: %fish -C 'set -g fish %fish' %s

set -g fish (realpath $fish)

# Store pwd to later go back before cleaning up
set -l oldpwd (pwd)

set -l tmp (mktemp -d)
cd $tmp
# resolve CDPATH (issue 6220)
begin
    mkdir -p x
    # CHECK: /{{.*}}/x
    cd x
    pwd

    # CHECK: /{{.*}}/x
    set -lx CDPATH ..
    cd x
    pwd
end
cd $oldpwd
rm -rf $tmp

# Create a test directory to store our stuff.
# macOS likes to return symlinks from (mktemp -d), make sure it does not.
set -l base (realpath (mktemp -d))
set real (realpath (mktemp -d))
set link $base/link
ln -s $real $link
cd $link
prevd
nextd
test "$PWD" = "$link" || echo "\$PWD != \$link:"\n "\$PWD: $PWD"\n "\$link: $link"\n
test (pwd) = "$link" || echo "(pwd) != \$link:"\n "\$PWD: "(pwd)\n "\$link: $link"\n
test (pwd -P) = "$real" || echo "(pwd -P) != \$real:"\n "\$PWD: $PWD"\n "\$real: $real"\n
test (pwd -P -L) = "$link" || echo "(pwd -P -L) != \$link:"\n "\$PWD: $PWD"\n "\$link: $link"\n
# Expect no output on success.

# Create a symlink and verify logical completion.
# create directory $base/through/the/looking/glass
# symlink $base/somewhere/teleport -> $base/through/the/looking/glass
# verify that .. completions work
cd $base
mkdir -p $base/through/the/looking/glass

mkdir -p $base/somewhere
mkdir $base/somewhere/a1
mkdir $base/somewhere/a2
mkdir $base/somewhere/a3
touch $base/through/the/looking/b(seq 1 3)
mkdir $base/through/the/looking/d1
mkdir $base/through/the/looking/d2
mkdir $base/through/the/looking/d3
ln -s $base/through/the/looking/glass $base/somewhere/rabbithole

cd $base/somewhere/rabbithole
echo "ls:"
complete -C'ls ../'
#CHECK: ls:
#CHECK: ../b1
#CHECK: ../b2
#CHECK: ../b3
#CHECK: ../d1/
#CHECK: ../d2/
#CHECK: ../d3/
#CHECK: ../glass/

echo "cd:"
complete -C'cd ../'
#CHECK: cd:
#CHECK: ../a1/
#CHECK: ../a2/
#CHECK: ../a3/
#CHECK: ../rabbithole/


# PWD should be imported and respected by fish
cd $oldpwd
mkdir -p $base/realhome
ln -s $base/realhome $base/linkhome
cd $base/linkhome
set -l real_getcwd (pwd -P)
env HOME=$base/linkhome $fish -c 'echo PWD is $PWD'
#CHECK: PWD is {{.*}}/linkhome


# Do not inherit a virtual PWD that fails to resolve to getcwd (#5647)
env HOME=$base/linkhome PWD=/tmp $fish -c 'echo $PWD' | read output_pwd
test (realpath $output_pwd) = $real_getcwd
and echo "BogusPWD test 1 succeeded"
or echo "BogusPWD test 1 failed: $output_pwd vs $real_getcwd"
#CHECK: BogusPWD test 1 succeeded

env HOME=$base/linkhome PWD=/path/to/nowhere $fish -c 'echo $PWD' | read output_pwd
test (realpath $output_pwd) = $real_getcwd
and echo "BogusPWD test 2 succeeded"
or echo "BogusPWD test 2 failed: $output_pwd vs $real_getcwd"
#CHECK: BogusPWD test 2 succeeded

# $CDPATH
set -g CDPATH $base
cd linkhome
test $PWD = $base/linkhome; and echo Gone to linkhome via CDPATH
#CHECK: Gone to linkhome via CDPATH

set -g CDPATH /tmp
cd $base
test $PWD = $base; and echo Gone to base
#CHECK: Gone to base

cd linkhome
test $PWD = $base/linkhome; and echo Gone to linkhome via implicit . in CDPATH
#CHECK: Gone to linkhome via implicit . in CDPATH

set -g CDPATH ./
cd $base
test $PWD = $base; and echo No crash with ./ CDPATH
#CHECK: No crash with ./ CDPATH

# test for directories beginning with a hyphen
mkdir $base/-testdir
cd $base
cd -- -testdir
test $PWD = $base/-testdir
echo $status
#CHECK: 0


# cd back before removing the test directory again.
cd $oldpwd
rm -Rf $base

# Verify that PWD on-variable events are sent
function __fish_test_changed_pwd --on-variable PWD
    echo "Changed to $PWD"
end
cd /
functions --erase __fish_test_changed_pwd
#CHECK: Changed to /

# Verify that cds don't stomp on each other.
function __fish_test_thrash_cd
    set -l dir (mktemp -d)
    cd $dir
    for i in (seq 50)
        test (/bin/pwd) = $dir
        and test $PWD = $dir
        or echo "cd test failed" 1>&2
        sleep .002
    end
end
__fish_test_thrash_cd |
__fish_test_thrash_cd |
__fish_test_thrash_cd |
__fish_test_thrash_cd |
__fish_test_thrash_cd
