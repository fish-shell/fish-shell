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

# CHECKERR: + for 1 2 3
# CHECKERR: ++ echo 1
# CHECKERR: ++ echo 2
# CHECKERR: ++ echo 3
# CHECKERR: + end for

while true
    and true
    echo inside
    break
end

# CHECK: inside

# CHECKERR: + while
# CHECKERR: + true
# CHECKERR: + true
# CHECKERR: ++ echo inside
# CHECKERR: ++ break
# CHECKERR: + end while

while true && true
    echo inside2
    break
end

# CHECK: inside2

# CHECKERR: + while
# CHECKERR: + true
# CHECKERR: + true
# CHECKERR: ++ echo inside2
# CHECKERR: ++ break
# CHECKERR: + end while

if true && false
else if false || true
    echo inside3
else if will_not_execute
end

# CHECK: inside3

# CHECKERR: + if
# CHECKERR: + true
# CHECKERR: + false
# CHECKERR: + else if
# CHECKERR: + false
# CHECKERR: + true
# CHECKERR: ++ echo inside3
# CHECKERR: + end if

set -e fish_trace
# CHECKERR: + set -e fish_trace

echo untraced
# CHECK: untraced
