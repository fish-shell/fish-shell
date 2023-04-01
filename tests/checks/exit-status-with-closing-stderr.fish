# RUN: %ghoti %s
argparse r-require= -- --require 2>/dev/null
echo $status
# CHECK: 2
argparse r-require= -- --require 2>&-
echo $status
# CHECK: 2
