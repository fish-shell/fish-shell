# RUN: %fish -C 'set -g fish %fish' %s

# caret position (#5812)
printf '<%s>\n' ($fish -c ' $f[a]' 2>&1)

# CHECK: <fish: Invalid index value>
# CHECK: < $f[a]>
# CHECK: <    ^>

printf '<%s>\n' ($fish -c 'if $f[a]; end' 2>&1)
# CHECK: <fish: Invalid index value>
# CHECK: <if $f[a]; end>
# CHECK: <      ^>

set a A
set aa AA
set aaa AAA
echo {$aa}a{1,2,3}(for a in 1 2 3; echo $a; end)
#CHECK: AAa11 AAa21 AAa31 AAa12 AAa22 AAa32 AAa13 AAa23 AAa33
