# RUN: %fish %s

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

#CHECK: Job	Group	CPU	State	Command
#CHECK: 3{{.*\t}}sleep 300 &
#CHECK: 2{{.*\t}}sleep 200 &
#CHECK: 1{{.*\t}}sleep 100 &

status job-control interactive

for pid in $tokill
    command kill -9 $pid
end
