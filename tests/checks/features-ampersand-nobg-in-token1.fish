#RUN: %ghoti --features=ampersand-nobg-in-token -C 'set -g ghoti_indent %ghoti_indent' %s

echo no&background
# CHECK: no&background

echo background&
# CHECK: background

echo background &
# CHECK: background

wait

echo no&bg | ghoti_features=ampersand-nobg-in-token $ghoti_indent --check
echo $status #CHECK: 0
