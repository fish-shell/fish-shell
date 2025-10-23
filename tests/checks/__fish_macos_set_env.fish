# RUN: %fish %s

mkdir etc
{
    echo /usr/share/man
    echo /usr/local/share/man
} >etc/manpaths
mkdir etc/manpaths.d

MANPATH=:/custom/man-path \
    __fish_macos_set_env MANPATH etc/manpaths etc/manpaths.d
string join \n entry=$MANPATH
# CHECK: entry=/usr/share/man
# CHECK: entry=/usr/local/share/man
# CHECK: entry=/custom/man-path
# CHECK: entry=
