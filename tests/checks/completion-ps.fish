# RUN: %fish %s
# REQUIRES: test "$(uname)" = Darwin
# REQUIRES: test "$(command -v ps)" = /bin/ps

set -l options (complete -C 'ps -' | string replace -r '\t.*' '')

for option in -A -E -g
    contains -- $option $options
    and echo $option
end

for option in -H -J -N -Z
    contains -- $option $options
    or echo "no $option"
end

# CHECK: -A
# CHECK: -E
# CHECK: -g
# CHECK: no -H
# CHECK: no -J
# CHECK: no -N
# CHECK: no -Z
