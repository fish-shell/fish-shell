#RUN: %fish --features=ampersand-nobg-in-token -C 'set -g fish_indent %fish_indent' %s

echo no&background
# CHECK: no&background

echo background&
# CHECK: background

echo background &
# CHECK: background

wait

echo no&bg | fish_features=ampersand-nobg-in-token $fish_indent --check
echo $status #CHECK: 0
