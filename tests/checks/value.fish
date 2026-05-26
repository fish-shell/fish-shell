#RUN: %fish %s

value foo
# CHECK: foo

count (value a b c)
# CHECK: 3

echo (value a b c)[2]
# CHECK: b

value
echo $status
# CHECK: 0

# unmatched globs expand to nothing
value this-file-does-not-exist-*.zzz
echo "status: $status"
# CHECK: status: 0

set result (value x y z)
echo $result[1] $result[3]
# CHECK: x z

builtin value direct
# CHECK: direct

count (value '' notempty)
# CHECK: 2
