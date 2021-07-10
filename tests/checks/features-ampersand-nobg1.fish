#RUN: %fish --features=ampersand-nobg -C 'set -g fish_indent %fish_indent' %s

echo foo&bar
# CHECK: foo&bar

echo foo&
# CHECK: foo

echo foo &
# CHECK: foo

wait

echo foo&bar | fish_features=ampersand-nobg $fish_indent --check
echo $status #CHECK: 0
