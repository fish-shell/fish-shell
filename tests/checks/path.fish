#RUN: %fish %s
# The "path" builtin for dealing with paths

# Extension - for figuring out the file extension of a given path.
path extension /
or echo None
# CHECK: None

# No extension
path extension /.
or echo Filename is just a dot, no extension
# CHECK: Filename is just a dot, no extension

# No extension - ".foo" is the filename
path extension /.foo
or echo None again
# CHECK: None again

path extension /foo
or echo None once more
# CHECK: None once more
path extension /foo.txt
and echo Success
# CHECK: .txt
# CHECK: Success
path extension /foo.txt/bar
or echo Not even here
# CHECK: Not even here
path extension . ..
or echo No extension
# CHECK: No extension
path extension ./foo.mp4
# CHECK: .mp4
path extension ../banana
# nothing, status 1
echo $status
# CHECK: 1
path extension ~/.config
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

path normalize /usr/bin//../../etc/fish
# The "//" is squashed and the ".." components neutralize the components before
# CHECK:  /etc/fish
path normalize /bin//bash
# The "//" is squashed, but /bin isn't resolved even if your system links it to /usr/bin.
# CHECK:  /bin/bash

# We need to remove the rest of the path because we have no idea what its value looks like.
path real bin//sh | string match -r -- 'bin/bash$'
# The "//" is squashed, and the symlink is resolved.
# sh here is bash
# CHECK: bin/bash
