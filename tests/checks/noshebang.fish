# RUN: %fish %s

# Test for shebangless scripts - see 7802.

set testdir (mktemp -d)
cd $testdir

touch file
chmod a+x file

function runfile
    # Run our file twice, printing status.
    # Arguments are passed to exercise the re-execve code paths; they have no other effect.
    true # clear status
    set -g fish_use_posix_spawn 1
    ./file arg1 arg2 arg3
    echo $status

    true # clear status
    set -g fish_use_posix_spawn 0
    ./file arg1 arg2 arg3 arg4 arg5
    echo $status
end

# Empty executable files are 'true'.
true >file
sleep 0.1
runfile
#CHECK: 0
#CHECK: 0

# Files without NUL are 'true' as well.
echo -e -n '#COMMENT\n#COMMENT' >file
runfile
#CHECK: 0
#CHECK: 0

# Never implicitly pass files ending with .fish to /bin/sh.
true >file.fish
sleep 0.1
chmod a+x file.fish
set -g fish_use_posix_spawn 0
./file.fish
#CHECKERR: exec: {{.*}}
#CHECKERR: exec: {{.*}}
echo $status
#CHECK: 126
set -g fish_use_posix_spawn 1
./file.fish
#CHECKERR: exec: {{.*}}
#CHECKERR: exec: {{.*}}
echo $status
#CHECK: 126
rm file.fish


# On to NUL bytes.
# The heuristic is that there must be a line containing a lowercase letter before the first NUL byte.
echo -n -e 'true\n\x00' >file
sleep 0.1
runfile
#CHECK: 0
#CHECK: 0

# Doesn't meet our heuristic as there is no newline.
echo -n -e 'true\x00' >file
sleep 0.1
runfile
#CHECK: 126
#CHECKERR: exec: {{.*}}
#CHECKERR: exec: {{.*}}

#CHECK: 126
#CHECKERR: exec: {{.*}}
#CHECKERR: exec: {{.*}}

# Doesn't meet our heuristic as there is no lowercase before newline.
echo -n -e 'NOPE\n\x00' >file
sleep 0.1
runfile
#CHECK: 126
#CHECKERR: exec: {{.*}}
#CHECKERR: exec: {{.*}}

#CHECK: 126
#CHECKERR: exec: {{.*}}
#CHECKERR: exec: {{.*}}

echo 'echo foo' >./-
sleep 0.1
chmod +x ./-
set PATH ./ $PATH
sleep 0.1
-
#CHECK: foo
echo $status
#CHECK: 0
