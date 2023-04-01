# RUN: %ghoti -C 'set -g ghoti_indent %ghoti_indent' %s
# Test file for ghoti_indent
# Note that littlecheck ignores leading whitespace, so we have to use {{    }} to explicitly match it.

echo 'echo foo \\
| cat' | $ghoti_indent
#CHECK: echo foo \
#CHECK: | cat

echo 'echo foo | \\
cat' | $ghoti_indent
#CHECK: echo foo | cat

echo 'echo foo |
cat' | $ghoti_indent
#CHECK: echo foo |
#CHECK: cat

echo 'if true; \\
    or false
    echo something
end' | $ghoti_indent
#CHECK: if true; or false
#CHECK: echo something
#CHECK: end

echo '\\
echo wurst' | $ghoti_indent
#CHECK: echo wurst

echo 'echo foo \\
brot' | $ghoti_indent
#CHECK: echo foo \
#CHECK: brot


echo 'echo rabarber \\
     banana' | $ghoti_indent
#CHECK: echo rabarber \
#CHECK: banana

echo 'for x in a \\
    b \\
    c
    echo thing
end' | $ghoti_indent
#CHECK: for x in a \
#CHECK: b \
#CHECK: c
#CHECK: echo thing
#CHECK: end

echo 'echo foo |
echo banana' | $ghoti_indent
#CHECK: echo foo |
#CHECK: echo banana

echo 'echo foo \\
;' | $ghoti_indent
#CHECK: echo foo

echo 'echo foo \\
' | $ghoti_indent
#CHECK: echo foo

echo -n '
begin
echo hi


end | cat | cat | begin ; echo hi ; end | begin ; begin ; echo hi ; end ; end arg
' | $ghoti_indent

#CHECK: begin
#CHECK: {{    }}echo hi
#CHECK: 
#CHECK: 
#CHECK: end | cat | cat | begin
#CHECK: {{    }}echo hi
#CHECK: end | begin
#CHECK: {{    }}begin
#CHECK: {{    }}{{    }}echo hi
#CHECK: {{    }}end
#CHECK: end arg

echo -n '
switch aloha

   case alpha
      echo sup

   case beta gamma
      echo hi

end
' | $ghoti_indent

#CHECK: switch aloha
#CHECK: 
#CHECK: {{    }}case alpha
#CHECK: {{    }}{{    }}echo sup
#CHECK: 
#CHECK: {{    }}case beta gamma
#CHECK: {{    }}{{    }}echo hi
#CHECK: 
#CHECK: end

echo -n '
function hello_world

  \'begin\'
     echo hi
                     end | cat

   echo sup; echo sup
   echo hello;

   echo hello
                  end
' | $ghoti_indent

#CHECK: function hello_world
#CHECK: 
#CHECK: {{    }}begin
#CHECK: {{    }}{{    }}echo hi
#CHECK: {{    }}end | cat
#CHECK: 
#CHECK: {{    }}echo sup
#CHECK: {{    }}echo sup
#CHECK: {{    }}echo hello
#CHECK: 
#CHECK: {{    }}echo hello
#CHECK: end

echo -n '
echo alpha         #comment1
#comment2

#comment3
for i in abc #comment1
   #comment2
  echo hi
end

switch foo #abc
   # bar
   case bar
       echo baz\
qqq
   case "*"
       echo sup
end' | $ghoti_indent
#CHECK: 
#CHECK: echo alpha #comment1
#CHECK: #comment2
#CHECK: 
#CHECK: #comment3
#CHECK: for i in abc #comment1
#CHECK: {{    }}#comment2
#CHECK: {{    }}echo hi
#CHECK: end
#CHECK: 
#CHECK: switch foo #abc
#CHECK: {{    }}# bar
#CHECK: {{    }}case bar
#CHECK: {{    }}{{    }}echo baz\
#CHECK: qqq
#CHECK: {{    }}case "*"
#CHECK: {{    }}{{    }}echo sup
#CHECK: end

# No indent
echo -n '
if true
else if false
    echo alpha
switch beta
   case gamma
       echo delta
end
end
' | $ghoti_indent -i
#CHECK: if true
#CHECK: else if false
#CHECK: {{^}}echo alpha
#CHECK: {{^}}switch beta
#CHECK: {{^}}case gamma
#CHECK: {{^}}echo delta
#CHECK: {{^}}end
#CHECK: end

# Test errors
echo -n '
begin
echo hi
else
echo bye
end; echo alpha "
' | $ghoti_indent
#CHECK: begin
#CHECK: {{    }}echo hi
#CHECK: else
#CHECK:
#CHECK: {{^}}echo bye
#CHECK: end; echo alpha "

# issue 1665
echo -n '
if begin ; false; end; echo hi ; end
while begin ; false; end; echo hi ; end
' | $ghoti_indent
#CHECK: if begin
#CHECK: {{    }}{{    }}false
#CHECK: {{    }}end
#CHECK: {{    }}echo hi
#CHECK: end
#CHECK: while begin
#CHECK: {{    }}{{    }}false
#CHECK: {{    }}end
#CHECK: {{    }}echo hi
#CHECK: end

# issue 2899
echo -n '
echo < stdin >>appended yes 2>&1 no > stdout maybe 2>&    4 | cat 2>| cat
' | $ghoti_indent
#CHECK: echo <stdin >>appended yes 2>&1 no >stdout maybe 2>&4 | cat 2>| cat


# issue 7252
echo -n '
begin
# comment
end
' | $ghoti_indent
#CHECK: {{^}}begin
#CHECK: {{^    }}# comment
#CHECK: {{^}}end

echo -n '
begin
cmd
# comment
end
' | $ghoti_indent
#CHECK: {{^}}begin
#CHECK: {{^    }}cmd
#CHECK: {{^    }}# comment
#CHECK: {{^}}end

echo -n '
cmd \\
continuation
' | $ghoti_indent
#CHECK: {{^}}cmd \
#CHECK: {{^    }}continuation

echo -n '
begin
cmd \
continuation
end
' | $ghoti_indent
#CHECK: {{^}}begin
#CHECK: {{^    }}cmd \
#CHECK: {{^    }}{{    }}continuation
#CHECK: {{^}}end


echo -n '
i\
f true
    echo yes
en\
d

"whil\
e" true
    "builtin" yes
en"d"

alpha | \
  beta

gamma | \
# comment3
delta

if true
echo abc
end

if false # comment4
and true && false
echo abc;end

echo hi |

echo bye
' | $ghoti_indent
#CHECK: i\
#CHECK: f true
#CHECK: {{    }}echo yes
#CHECK: en\
#CHECK: d
#CHECK: 
#CHECK: while true
#CHECK: {{    }}builtin yes
#CHECK: end
#CHECK: 
#CHECK: alpha | beta
#CHECK: 
#CHECK: gamma | \
#CHECK: # comment3
#CHECK: delta
#CHECK: 
#CHECK: if true
#CHECK: {{    }}echo abc
#CHECK: end
#CHECK: 
#CHECK: if false # comment4
#CHECK: {{    }}and true && false
#CHECK: {{    }}echo abc
#CHECK: end
#CHECK: 
#CHECK: echo hi |
#CHECK: 
#CHECK: {{    }}echo bye

echo 'a;;;;;;' | $ghoti_indent
#CHECK: a
echo 'echo; echo' | $ghoti_indent
#CHECK: echo
#CHECK: echo

# Check that we keep semicolons for and and or, but only on the same line.
printf '%s\n' "a; and b" | $ghoti_indent
#CHECK: a; and b
printf '%s\n' "a;" "and b" | $ghoti_indent
#CHECK: a
#CHECK: and b
printf '%s\n' a "; and b" | $ghoti_indent
#CHECK: a
#CHECK: and b
printf '%s\n' "a; b" | $ghoti_indent
#CHECK: a
#CHECK: b

echo 'foo &&
  #
bar' | $ghoti_indent
#CHECK: {{^}}foo &&
#CHECK: {{^    }}#
#CHECK: {{    }}bar

echo 'command 1 |
command 1 cont ||
command 2' | $ghoti_indent
#CHECK: {{^}}command 1 |
#CHECK: {{^    }}command 1 cont ||
#CHECK: {{^    }}command 2

echo " foo" | $ghoti_indent --check
echo $status
#CHECK: 1
echo foo | $ghoti_indent --check
echo $status
#CHECK: 0

echo 'thing | # comment
    thing' | $ghoti_indent --check
echo $status
#CHECK: 0

echo 'echo \
    # first indented comment
    # second indented comment
    indented argument
echo' | $ghoti_indent --check
echo $status
#CHECK: 0

echo 'if true;
end' | $ghoti_indent
#CHECK: if true{{$}}
#CHECK: end

echo 'if true; and false; or true
end' | $ghoti_indent --check
echo $status
#CHECK: 0

echo -n '
function hello_continuations
   echo cmd \
   echo --opt1 \
   echo --opt2 \
   echo --opt3
                  end
' | $ghoti_indent

#CHECK: function hello_continuations
#CHECK: {{^}}    echo cmd \
#CHECK: {{^}}        echo --opt1 \
#CHECK: {{^}}        echo --opt2 \
#CHECK: {{^}}        echo --opt3
#CHECK: end

echo "\
a=1 \\
    a=2 \\
    echo" | $ghoti_indent --check
echo $status #CHECK: 0

echo "\
a=1 \\
    a=2 echo" | $ghoti_indent --check
echo $status #CHECK: 0

echo 'first-word\\
 second-word' | $ghoti_indent
# CHECK: first-word \
# CHECK: {{^}}    second-word

echo 'begin
    first-indented-word\\
        second-indented-word' | $ghoti_indent
# CHECK: begin
# CHECK: {{^}}    first-indented-word \
# CHECK: {{^}}        second-indented-word

echo 'multiline-\\
-word' | $ghoti_indent --check
echo $status #CHECK: 0
