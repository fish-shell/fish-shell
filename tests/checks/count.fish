#RUN: %ghoti %s
# Validate the behavior of the `count` command.

# no args
count
# CHECK: 0

# one args
count x
# CHECK: 1

# two args
count x y
# CHECK: 2

# args that look like flags or are otherwise special
count -h
# CHECK: 1
count --help
# CHECK: 1
count --
# CHECK: 1
count -- abc
# CHECK: 2
count def -- abc
# CHECK: 3

# big counts

# See #5611
for i in seq 500
    set c1 (count (seq 1 10000))
    test $c1 -eq 10000
    or begin
        echo "Count $c1 is not 10000"
        break
    end
end

# stdin
# Reading from stdin still counts the arguments
printf '%s\n' 1 2 3 4 5 | count 6 7 8 9 10
# CHECK: 10

# Reading from stdin counts newlines - like `wc -l`.
echo -n 0 | count
# CHECK: 0

echo 1 | count
# CHECK: 1
