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
