#RUN: %fish --features=ampersand-nobg-in-token %s

echo no&background
# CHECK: no&background

echo background&
# CHECK: background

echo background &
# CHECK: background

wait
