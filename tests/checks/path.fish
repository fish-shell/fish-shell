#RUN: %fish %s
# The "path" builtin for dealing with paths

# Extension - for figuring out the file extension of a given path.
path extension /
or echo None
# CHECK:
# CHECK: None

# No extension
path extension /.
or echo Filename is just a dot, no extension
# CHECK:
# CHECK: Filename is just a dot, no extension

# No extension - ".foo" is the filename
path extension /.foo
or echo None again
# CHECK:
# CHECK: None again

path extension /foo
or echo None once more
# CHECK:
# CHECK: None once more
path extension /foo.txt
and echo Success
# CHECK: .txt
# CHECK: Success
path extension /foo.txt/bar
or echo Not even here
# CHECK:
# CHECK: Not even here
path extension . ..
or echo No extension
# CHECK:
# CHECK: No extension
path extension ./foo.mp4
# CHECK: .mp4
path extension ../banana
# CHECK:
# nothing, status 1
echo $status
# CHECK: 1
path extension ~/.config
# CHECK:
# nothing, status 1
echo $status
# CHECK: 1
path extension ~/.config.d
# CHECK: .d
path extension ~/.config.
echo $status
# status 0
# CHECK: .
# CHECK: 0

path change-extension '' ./foo.mp4
# CHECK: ./foo
path change-extension wmv ./foo.mp4
# CHECK: ./foo.wmv
path change-extension .wmv ./foo.mp4
# CHECK: ./foo.wmv
path change-extension '' ../banana
# CHECK: ../banana
# still status 0, because there was an argument
echo $status
# CHECK: 0
path change-extension '' ~/.config
# CHECK: {{.*}}/.config
echo $status
# CHECK: 0

path basename ./foo.mp4
# CHECK: foo.mp4
path basename ../banana
# CHECK: banana
path basename /usr/bin/
# CHECK: bin
path dirname ./foo.mp4
# CHECK: .
path basename ../banana
# CHECK: banana
path basename /usr/bin/
# CHECK: bin

cd $TMPDIR
mkdir -p bin
touch bin/{bash,bssh,chsh,dash,fish,slsh,ssh,zsh}
ln -s $TMPDIR/bin/bash bin/sh

chmod +x bin/*
# We need files from here on
path filter bin argagagji
# The (hopefully) nonexistent argagagji is filtered implicitly:
# CHECK: bin

# With --invert, the existing bin is filtered
path filter --invert bin argagagji
# CHECK: argagagji

# With --invert and a type, bin fails the type,
# and argagagji doesn't exist, so both are printed.
path filter -vf bin argagagji
# CHECK: bin
# CHECK: argagagji

# With --all, return true if all paths are passed.
path filter --all bin bin/bash
echo $status
# CHECK: 0
path filter --all bin argagagji
echo $status
# CHECK: 1
# With --all and --invert, return true if none of paths is passed.
path filter --all --invert bin bin/bash
echo $status
# CHECK: 1
path filter --all --invert argagagji argagagji2
echo $status
# CHECK: 0

path filter --type file bin bin/fish
# Only fish is a file
# CHECK: bin/fish
chmod 500 bin/fish
path filter --type file,dir --perm exec,write bin/fish .
# fish is a file, which passes, and executable, which passes,
# but not writable, which fails.
#
# . is a directory and both writable and executable, typically.
# So it passes.
# CHECK: .

mkdir -p sbin
touch sbin/setuid-exe sbin/setgid-exe
chmod u+s,a+x sbin/setuid-exe
path filter --perm suid sbin/*
# CHECK: sbin/setuid-exe

# On at least FreeBSD on our CI this fails with "permission denied".
# So we can't test it, and we fake the output instead.
if chmod g+s,a+x sbin/setgid-exe 2>/dev/null
    path filter --perm sgid sbin/*
else
    echo sbin/setgid-exe
end
# CHECK: sbin/setgid-exe

mkdir stuff
touch stuff/{read,write,exec,readwrite,readexec,writeexec,all,none}
chmod 400 stuff/read
chmod 200 stuff/write
chmod 100 stuff/exec
chmod 600 stuff/readwrite
chmod 500 stuff/readexec
chmod 300 stuff/writeexec
chmod 700 stuff/all
chmod 000 stuff/none

# Validate that globs are sorted.
test (path filter stuff/* | path sort | string join ",") = (path filter stuff/* | string join ",")

path filter --perm read stuff/*
# CHECK: stuff/all
# CHECK: stuff/read
# CHECK: stuff/readexec
# CHECK: stuff/readwrite

path filter -r stuff/*
# CHECK: stuff/all
# CHECK: stuff/read
# CHECK: stuff/readexec
# CHECK: stuff/readwrite

path filter --perm write stuff/*
# CHECK: stuff/all
# CHECK: stuff/readwrite
# CHECK: stuff/write
# CHECK: stuff/writeexec

path filter -w stuff/*
# CHECK: stuff/all
# CHECK: stuff/readwrite
# CHECK: stuff/write
# CHECK: stuff/writeexec

path filter --perm exec stuff/*
# CHECK: stuff/all
# CHECK: stuff/exec
# CHECK: stuff/readexec
# CHECK: stuff/writeexec

path filter -x stuff/*
# CHECK: stuff/all
# CHECK: stuff/exec
# CHECK: stuff/readexec
# CHECK: stuff/writeexec

path filter --perm read,write stuff/*
# CHECK: stuff/all
# CHECK: stuff/readwrite

path filter --perm read,exec stuff/*
# CHECK: stuff/all
# CHECK: stuff/readexec

path filter --perm write,exec stuff/*
# CHECK: stuff/all
# CHECK: stuff/writeexec

path filter --perm read,write,exec stuff/*
# CHECK: stuff/all

path filter stuff/*
# CHECK: stuff/all
# CHECK: stuff/exec
# CHECK: stuff/none
# CHECK: stuff/read
# CHECK: stuff/readexec
# CHECK: stuff/readwrite
# CHECK: stuff/write
# CHECK: stuff/writeexec

path normalize /usr/bin//../../etc/fish
# The "//" is squashed and the ".." components neutralize the components before
# CHECK:  /etc/fish
path normalize /bin//bash
# The "//" is squashed, but /bin isn't resolved even if your system links it to /usr/bin.
# CHECK:  /bin/bash

# Paths with "-" get a "./":
path normalize -- -/foo -foo/foo
# CHECK: ./-/foo
# CHECK: ./-foo/foo
path normalize -- ../-foo
# CHECK: ../-foo

# This goes for filter as well
touch -- -foo
path filter -f -- -foo
# CHECK: ./-foo

# We need to remove the rest of the path because we have no idea what its value looks like.
path resolve bin//sh | string match -r -- 'bin/bash$'
# The "//" is squashed, and the symlink is resolved.
# sh here is bash
# CHECK: bin/bash

# "../" cancels out even files.
path resolve bin//sh/../ | string match -r -- 'bin$'
# CHECK: bin

# `path resolve` with nonexistent paths
set -l path (path resolve foo/bar)
string match -rq "^"(pwd -P | string escape --style=regex)'/' -- $path
and echo It matches pwd!
or echo pwd is \'$PWD\' resolved path is \'$path\'
# CHECK: It matches pwd!
string replace -r "^"(pwd -P | string escape --style=regex)'/' "" -- $path
# CHECK: foo/bar

path resolve /banana//terracota/terracota/booooo/../pie
# CHECK: /banana/terracota/terracota/pie

path sort --key=basename {def,abc}/{456,123,789,abc,def,0} | path sort --key=dirname -r
# CHECK: def/0
# CHECK: def/123
# CHECK: def/456
# CHECK: def/789
# CHECK: def/abc
# CHECK: def/def
# CHECK: abc/0
# CHECK: abc/123
# CHECK: abc/456
# CHECK: abc/789
# CHECK: abc/abc
# CHECK: abc/def

path sort --unique --key=basename {def,abc}/{456,123,789} def/{abc,def,0} abc/{foo,bar,baz}
# CHECK: def/0
# CHECK: def/123
# CHECK: def/456
# CHECK: def/789
# CHECK: def/abc
# CHECK: abc/bar
# CHECK: abc/baz
# CHECK: def/def
# CHECK: abc/foo

# Symlink loop.
# It goes brrr.
ln -s target link
ln -s link target

test (path resolve target) = (pwd -P)/target
and echo target resolves to target
# CHECK: target resolves to target

test (path resolve link) = (pwd -P)/link
and echo link resolves to link
# CHECK: link resolves to link

# path mtime
# These tests deal with *time*, so we have to account
# for slow systems (like CI).
# So we should only test with a lot of slack.

echo bananana >>foo
test (math abs (date +%s) - (path mtime foo)) -lt 20
or echo MTIME IS BOGUS

sleep 2

set -l mtime (path mtime --relative foo)
test $mtime -ge 1
or echo mtime is too small

test $mtime -lt 20
or echo mtime is too large

touch -m -t 197001020000 epoch
set -l epochtime (path mtime epoch)
# Allow for timezone shenanigans
test $epochtime -gt 0 -a $epochtime -lt 180000
or echo Oops not mtime

path basename -Z foo bar baz | path sort
# CHECK: bar
# CHECK: baz
# CHECK: foo

path basename -E foo.txt /usr/local/foo.bar /foo.tar.gz
# CHECK: foo
# CHECK: foo
# CHECK: foo.tar

path basename --null-out bar baz | string escape
# CHECK: bar\x00baz\x00
