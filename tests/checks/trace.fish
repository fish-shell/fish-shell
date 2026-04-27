# RUN: %fish %s

echo untraced
# CHECK: untraced

set fish_trace 1

for i in 1 2 3
    echo $i
end

# CHECK: 1
# CHECK: 2
# CHECK: 3

# CHECKERR: > for 1 2 3
# CHECKERR: -> echo 1
# CHECKERR: -> echo 2
# CHECKERR: -> echo 3
# CHECKERR: > end for

while true
    and true
    echo inside
    break
end

# CHECK: inside

# CHECKERR: > while
# CHECKERR: > true
# CHECKERR: > true
# CHECKERR: -> echo inside
# CHECKERR: -> break
# CHECKERR: > end while

while true && true
    echo inside2
    break
end

# CHECK: inside2

# CHECKERR: > while
# CHECKERR: > true
# CHECKERR: > true
# CHECKERR: -> echo inside2
# CHECKERR: -> break
# CHECKERR: > end while

if true && false
else if false || true
    echo inside3
else if will_not_execute
end

# CHECK: inside3

# CHECKERR: > if
# CHECKERR: > true
# CHECKERR: > false
# CHECKERR: > else if
# CHECKERR: > false
# CHECKERR: > true
# CHECKERR: -> echo inside3
# CHECKERR: > end if

# Now check tracing with a limited trace depth.
# First, we set depth high enough to show everything.

set fish_trace_depth 2
# CHECKERR: > set fish_trace_depth 2

for i in 1 2
    for j in a b
        echo $i $j
    end
end

# CHECK: 1 a
# CHECK: 1 b
# CHECK: 2 a
# CHECK: 2 b

# CHECKERR: > for 1 2
# CHECKERR: -> for a b
# CHECKERR: --> echo 1 a
# CHECKERR: --> echo 1 b
# CHECKERR: -> end for
# CHECKERR: -> for a b
# CHECKERR: --> echo 2 a
# CHECKERR: --> echo 2 b
# CHECKERR: -> end for
# CHECKERR: > end for

# Next, we limit depth by one step so it doesn't show the inner commands.

set fish_trace_depth 1
# CHECKERR: > set fish_trace_depth 1

for i in 1 2
    for j in a b
        echo $i $j
    end
end

# CHECK: 1 a
# CHECK: 1 b
# CHECK: 2 a
# CHECK: 2 b

# CHECKERR: > for 1 2
# CHECKERR: -> for a b
# CHECKERR: -> end for
# CHECKERR: -> for a b
# CHECKERR: -> end for
# CHECKERR: > end for

# Finally, we limit depth fully so it only shows the outermost commands.

set fish_trace_depth 0
# CHECKERR: > set fish_trace_depth 0

for i in 1 2
    for j in a b
        echo $i $j
    end
end

# CHECK: 1 a
# CHECK: 1 b
# CHECK: 2 a
# CHECK: 2 b

# CHECKERR: > for 1 2
# CHECKERR: > end for

# Now test setting the depth based on a specific context we care about.

set fish_trace_depth 0
# CHECKERR: > set fish_trace_depth 0

for i in 1 2 3
    if test "$i" -eq 2
        set fish_trace_depth 10
    else
        set fish_trace_depth 0
    end

    for j in a b
        echo $i $j
    end
end

# CHECK: 1 a
# CHECK: 1 b
# CHECK: 2 a
# CHECK: 2 b
# CHECK: 3 a
# CHECK: 3 b

# i=1 only shows the outermost command because depth stays 0
# CHECKERR: > for 1 2 3

# i=2 picks up again after setting depth to 10
# CHECKERR: -> end if
# CHECKERR: -> for a b
# CHECKERR: --> echo 2 a
# CHECKERR: --> echo 2 b
# CHECKERR: -> end for

# i=3 shows depth being set back to 0 and then no more inside the loop
# CHECKERR: -> if
# CHECKERR: -> test 3 -eq 2
# CHECKERR: -> else
# CHECKERR: --> set fish_trace_depth 0
# CHECKERR: > end for

# Now, test increasing the depth after exceeding the previous depth. We set
# depth in a local scope so it's automatically reset.

set fish_trace_depth 1
# CHECKERR: > set fish_trace_depth 1

if true
    if true
        if true
            set --local fish_trace_depth 10
            if true
                true
            end
        end
    end
end

# These print because of the initial depth of 1.
# CHECKERR: > if
# CHECKERR: > true
# CHECKERR: -> if
# CHECKERR: -> true
# Then we jump to the entries that only print after depth is set to 10.
# CHECKERR: ---> if
# CHECKERR: ---> true
# CHECKERR: ----> true
# CHECKERR: ---> end if
# The locally set depth of 10 goes out of scope here, so we don't see the
# intermediate "end if", only the entries from the original depth of 1.
# CHECKERR: -> end if
# CHECKERR: > end if

# Test some invalid depth values. They should be ignored, and tracing should
# proceed as if no depth was given, i.e. show everything.

# Set depth to default first:
set -e fish_trace_depth
# CHECKERR: > set -e fish_trace_depth

set fish_trace_depth a
# CHECKERR: > set fish_trace_depth a

if true
    if true
        if true
        end
    end
end

# CHECKERR: > if
# CHECKERR: > true
# CHECKERR: -> if
# CHECKERR: -> true
# CHECKERR: --> if
# CHECKERR: --> true
# CHECKERR: --> end if
# CHECKERR: -> end if
# CHECKERR: > end if

set fish_trace_depth -1
# CHECKERR: > set fish_trace_depth -1

if true
    if true
        if true
        end
    end
end

# CHECKERR: > if
# CHECKERR: > true
# CHECKERR: -> if
# CHECKERR: -> true
# CHECKERR: --> if
# CHECKERR: --> true
# CHECKERR: --> end if
# CHECKERR: -> end if
# CHECKERR: > end if

# u64 max + 1
set fish_trace_depth 18446744073709551616
# CHECKERR: > set fish_trace_depth 18446744073709551616

if true
    if true
        if true
        end
    end
end

# CHECKERR: > if
# CHECKERR: > true
# CHECKERR: -> if
# CHECKERR: -> true
# CHECKERR: --> if
# CHECKERR: --> true
# CHECKERR: --> end if
# CHECKERR: -> end if
# CHECKERR: > end if

set fish_trace_depth ''
# CHECKERR: > set fish_trace_depth ''

if true
    if true
        if true
        end
    end
end

# CHECKERR: > if
# CHECKERR: > true
# CHECKERR: -> if
# CHECKERR: -> true
# CHECKERR: --> if
# CHECKERR: --> true
# CHECKERR: --> end if
# CHECKERR: -> end if
# CHECKERR: > end if

# Finally, make sure we can end tracing and see no more stderr.

set -e fish_trace
# CHECKERR: > set -e fish_trace

echo untraced
# CHECK: untraced
