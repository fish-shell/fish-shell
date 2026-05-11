# RUN: fish=%fish %fish %s
# Test the umask command. In particular the symbolic modes since they've been
# broken for four years (see issue #738) at the time I added these tests.

# When a path is mounted with `noacl` on Cygwin, file permissions are simulated
# in part based on the umask value.
# So masking the user execution bit when /usr/bin is mounted `noacl` will also
# prevent standard commands from executing, including `/usr/bin/cut` used by
# the `umask` function.
cygwin_noacl /usr/bin/ && set noacl

# Establish a base line umask.
umask 027
umask
echo umask var = $umask
umask -S
#CHECK: 0027
#CHECK: umask var = 0027
#CHECK: u=rwx,g=rx,o=

# Verify that an invalid umask is rejected
umask 1234
umask 228
umask 0282
#CHECKERR: umask: Invalid mask '1234'
#CHECKERR: umask: Invalid mask '228'
#CHECKERR: umask: Invalid mask '0282'

# Verify that symbolic modifications and output is correct.
#
# When I wrote these tests I based all of the results on the behavior of bash
# when executing identical commands. So if bash has a bug with the umask
# command it's possible fish will as well. However, I did verify the result of
# each interaction and did not find any bugs in how bash or fish handled these
# scenarios.
#
if set -q noacl
    echo 0777
    echo u=,g=,o=
else
    umask 0777
    umask a-r
    umask
    umask -S
end
#CHECK: 0777
#CHECK: u=,g=,o=

umask 0777
umask u+x
umask
umask -S
#CHECK: 0677
#CHECK: u=x,g=,o=

if set -q noacl
    echo 0706
    echo u=,g=rwx,o=x
else
    umask 777
    umask g+rwx,o+x
    umask
    umask -S
end
#CHECK: 0706
#CHECK: u=,g=rwx,o=x

umask 0
umask u-w,o-x
umask
umask -S
#CHECK: 0201
#CHECK: u=rx,g=rwx,o=rw

umask 0
umask a-r
umask
umask -S
#CHECK: 0444
#CHECK: u=wx,g=wx,o=wx

if set -q noacl
    echo 0550
    echo u=w,g=w,o=rwx
else
    umask 0
    umask ug-rx
    umask
    umask -S
end
#CHECK: 0550
#CHECK: u=w,g=w,o=rwx

if set -q noacl
    echo 0351
    echo u=r,g=w,o=rw
else
    umask 777
    umask u+r,g+w,o=rw
    umask
    umask -S
end
#CHECK: 0351
#CHECK: u=r,g=w,o=rw

if set -q noacl
    echo 0316
    echo u=r,g=rw,o=x
else
    umask 777
    umask =r,g+w,o+x,o-r
    umask
    umask -S
end
#CHECK: 0316
#CHECK: u=r,g=rw,o=x

umask 0
umask rx
umask
umask -S
#CHECK: 0222
#CHECK: u=rx,g=rx,o=rx

umask u=rwx,g=rwx,o=
umask
#CHECK: 0007
umask u=rwx,g=,o=rwx
umask
#CHECK: 0070
