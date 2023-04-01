# RUN: %ghoti %s

# Ensure that job IDs are sequential.

status job-control full

set -g tokill

function func100
    sleep 100 &
    set -g tokill $tokill $last_pid
end
func100

# The redirection ensures this becomes a real job.
begin
    sleep 200 &
    set -g tokill $tokill $last_pid
end </dev/null


sleep 300 &
set -g tokill $tokill $last_pid

jobs

#CHECK: Job	Group{{.*}}
#CHECK: 3{{.*\t}}sleep 300 &
#CHECK: 2{{.*\t}}sleep 200 &
#CHECK: 1{{.*\t}}sleep 100 &

# Kill job 2; the next job should have job ID 4 because 3 is still in use (#6053).

status job-control interactive
command kill -9 $tokill[2]
# Wait for the job to die - the signal needs to be delivered.
wait $tokill[2] 2>/dev/null
set -e tokill[2]
status job-control full
sleep 400 &
set -g tokill $tokill $last_pid

jobs

#CHECK: Job	Group{{.*}}
#CHECK: 4{{.*\t}}sleep 400 &
#CHECK: 3{{.*\t}}sleep 300 &
#CHECK: 1{{.*\t}}sleep 100 &


status job-control interactive

for pid in $tokill
    command kill -9 $pid
end
