#RUN: echo 'status job-control full; command echo A ; echo B;' | %ghoti

# Regression test for #6573.

#CHECK: A
#CHECK: B
