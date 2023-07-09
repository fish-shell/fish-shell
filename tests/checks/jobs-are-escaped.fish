# RUN: %fish %s

# Ensure that jobs are printed with new lines escaped

sleep \
100 &

jobs
#CHECK: Job	Group{{.*}}
# CHECK: 1{{.*\t}}sleep \\\n100 &
kill %1
