# RUN: %fish %s

disown 0
# CHECKERR: disown: '0' is not a valid process ID
disown -- -1
# CHECKERR: disown: '-1' is not a valid process ID
disown -- -(math 2 ^ 31)
# CHECKERR: disown: '-2147483648' is not a valid process ID
disown
# CHECKERR: disown: There are no suitable jobs

# So jobs can be resumed by disown
status job-control full
sleep 1 &
set -l pid (jobs -lp)
kill -SIGSTOP $pid
while jobs -l | grep -q running
    sleep .01
end
disown
# CHECKERR: disown: job 1 ('sleep 1 &') was stopped and has been signalled to continue.
echo $status
# CHECK: 0

# Regression test for #13113: autoloading disown must define disown instead of
# overwriting wait, and it must expand job specifications.
functions -q disown
echo $status
# CHECK: 0

sleep 1 &
disown %1
echo $status
# CHECK: 0
