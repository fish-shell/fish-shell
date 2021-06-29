#RUN: %fish --features=ampersand-nobg %s

echo foo&bar
# CHECK: foo&bar

echo foo&
# CHECK: foo

echo foo &
# CHECK: foo

wait
