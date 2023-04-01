#RUN: %ghoti -c '%ghoti -c false; echo RC: $status'
# CHECK: RC: 1
