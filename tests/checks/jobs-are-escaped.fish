# RUN: %fish %s

# Ensure that jobs are printed with new lines escaped

#!fish_indent: off
sleep \
100 &
#!fish_indent: on

jobs
#CHECK: Job	Group{{.*}}
# CHECK: 1{{.*\t}}sleep \\\n100 &
kill %1
