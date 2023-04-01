# RUN: %ghoti -C 'set -g ghoti %ghoti' %s
# Test the umask command. In particular the symbolic modes since they've been
# broken for four years (see issue #738) at the time I added these tests.

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
# command it's possible ghoti will as well. However, I did verify the result of
# each interaction and did not find any bugs in how bash or ghoti handled these
# scenarios.
#
umask 0777
umask a-r
umask
umask -S
#CHECK: 0777
#CHECK: u=,g=,o=

umask 0777
umask u+x
umask
umask -S
#CHECK: 0677
#CHECK: u=x,g=,o=

umask 777
umask g+rwx,o+x
umask
umask -S
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

umask 0
umask ug-rx
umask
umask -S
#CHECK: 0550
#CHECK: u=w,g=w,o=rwx

umask 777
umask u+r,g+w,o=rw
umask
umask -S
#CHECK: 0351
#CHECK: u=r,g=w,o=rw

umask 777
umask =r,g+w,o+x,o-r
umask
umask -S
#CHECK: 0316
#CHECK: u=r,g=rw,o=x

umask 0
umask rx
umask
umask -S
#CHECK: 0222
#CHECK: u=rx,g=rx,o=rx
