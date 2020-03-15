# RUN: %fish -C 'set -g fish_indent %fish_indent' %s
# Test file for fish_indent

echo 'echo foo \\
| cat' | $fish_indent
#CHECK: echo foo \
#CHECK: | cat

echo 'echo foo | \\
cat' | $fish_indent
#CHECK: echo foo | cat

echo 'echo foo |
cat' | $fish_indent
#CHECK: echo foo |
#CHECK: cat

echo 'if true; \\
    or false
    echo something
end' | $fish_indent
#CHECK: if true; or false
#CHECK: echo something
#CHECK: end

echo '\\
echo wurst' | $fish_indent
#CHECK: echo wurst

echo 'echo foo \\
brot' | $fish_indent
#CHECK: echo foo \
#CHECK: brot


echo 'echo rabarber \\
     banana' | $fish_indent
#CHECK: echo rabarber \
#CHECK: banana

echo 'for x in a \\
    b \\
    c
    echo thing
end' | $fish_indent
#CHECK: for x in a \
#CHECK: b \
#CHECK: c
#CHECK: echo thing
#CHECK: end
    
echo 'echo foo |
echo banana' | $fish_indent
#CHECK: echo foo |
#CHECK: echo banana

echo 'echo foo \\
;' | $fish_indent
#CHECK: echo foo \
#CHECK: 

echo 'echo foo \\
' | $fish_indent
#CHECK: echo foo \
