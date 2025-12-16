# RUN: fish_indent=%fish_indent %fish %s
# Test file for fish_indent
# Note that littlecheck ignores leading whitespace, so we have to use {{    }} to explicitly match it.

fish_indent --no-such-option
#CHECKERR: fish_indent: --no-such-option: unknown option

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
#CHECK: echo foo

echo 'echo foo \\
' | $fish_indent
#CHECK: echo foo

echo -n '
begin
echo hi


end | cat | cat | begin ; echo hi ; end | begin ; begin ; echo hi ; end ; end arg
' | $fish_indent

#CHECK: begin
#CHECK: {{    }}echo hi
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
' | $fish_indent

#CHECK: switch aloha
#CHECK: {{    }}case alpha
#CHECK: {{    }}{{    }}echo sup
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
' | $fish_indent

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
end' | $fish_indent
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
' | $fish_indent -i
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
' | $fish_indent
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
' | $fish_indent
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
' | $fish_indent
#CHECK: echo <stdin >>appended yes 2>&1 no >stdout maybe 2>&4 | cat 2>| cat


# issue 7252
echo -n '
begin
# comment
end
' | $fish_indent
#CHECK: {{^}}begin
#CHECK: {{^    }}# comment
#CHECK: {{^}}end

echo -n '
begin
cmd
# comment
end
' | $fish_indent
#CHECK: {{^}}begin
#CHECK: {{^    }}cmd
#CHECK: {{^    }}# comment
#CHECK: {{^}}end

echo -n '
cmd \\
continuation
' | $fish_indent
#CHECK: {{^}}cmd \
#CHECK: {{^    }}continuation

echo -n '
begin
cmd \
continuation
end
' | $fish_indent
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
' | $fish_indent
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
#CHECK: {{    }}echo bye

echo 'a;;;;;;' | $fish_indent
#CHECK: a
echo 'echo; echo' | $fish_indent
#CHECK: echo
#CHECK: echo

# Check that we keep semicolons for and and or, but only on the same line.
printf '%s\n' "a; and b" | $fish_indent
#CHECK: a; and b
printf '%s\n' "a;" "and b" | $fish_indent
#CHECK: a
#CHECK: and b
printf '%s\n' a "; and b" | $fish_indent
#CHECK: a
#CHECK: and b
printf '%s\n' "a; b" | $fish_indent
#CHECK: a
#CHECK: b

echo 'foo &&
  #
bar' | $fish_indent
#CHECK: {{^}}foo &&
#CHECK: {{^    }}#
#CHECK: {{    }}bar

echo 'command 1 |
command 1 cont ||
command 2' | $fish_indent
#CHECK: {{^}}command 1 |
#CHECK: {{^    }}command 1 cont ||
#CHECK: {{^    }}command 2

echo " foo" | $fish_indent --check
echo $status
#CHECK: 1
echo foo | $fish_indent --check
echo $status
#CHECK: 0

echo 'thing | # comment
    thing' | $fish_indent --check
echo $status
#CHECK: 0

echo 'echo \
    # first indented comment
    # second indented comment
    indented argument
echo' | $fish_indent --check
echo $status
#CHECK: 0

echo 'if true;
end' | $fish_indent
#CHECK: if true{{$}}
#CHECK: end

echo 'if true; and false; or true
end' | $fish_indent --check
echo $status
#CHECK: 0

echo -n '
function hello_continuations
   echo cmd \
   echo --opt1 \
   echo --opt2 \
   echo --opt3
                  end
' | $fish_indent

#CHECK: function hello_continuations
#CHECK: {{^}}    echo cmd \
#CHECK: {{^}}        echo --opt1 \
#CHECK: {{^}}        echo --opt2 \
#CHECK: {{^}}        echo --opt3
#CHECK: end

echo "\
a=1 \\
    a=2 \\
    echo" | $fish_indent --check
echo $status #CHECK: 0

echo "\
a=1 \\
    a=2 echo" | $fish_indent --check
echo $status #CHECK: 0

echo 'first-word\\
 second-word' | $fish_indent
# CHECK: first-word \
# CHECK: {{^}}    second-word

echo 'begin
    first-indented-word\\
        second-indented-word' | $fish_indent
# CHECK: begin
# CHECK: {{^}}    first-indented-word \
# CHECK: {{^}}        second-indented-word

{
    echo '{ no semi }'
    # CHECK: { no semi }
    echo '{ semi; }'
    # CHECK: { semi; }

    echo '{ multi; no semi }'
    # CHECK: { multi; no semi }
    echo '{ multi; semi; }'
    # CHECK: { multi; semi; }

    echo '{ conj && no semi }'
    # CHECK: { conj && no semi }
    echo '{ conj && semi; }'
    # CHECK: { conj && semi; }

    echo '{ }'
    # CHECK: { }
    echo '{ ; }'
    # CHECK: { }

    echo '
{
echo \\
# continuation comment
}'
    # CHECK: {
    # CHECK: {{^    }}echo \
    # CHECK: {{^        }}# continuation comment
    # TODO: This is currently broken; so this the begin/end equivalent.
    # CHECK: {{^    [}]}}

    echo '{  {  }  }'
    # CHECK: { { } }

    echo '{{}}'
    # CHECK: { { } }

    echo '
{

{
}

}
'
    # CHECK: {{^\{$}}
    # CHECK: {{^    \{$}}
    # CHECK: {{^    \}$}}
    # CHECK: {{^\}$}}

    echo '
{ level 1; {
level 2 } }
'
    # TODO Should add a line break here.
    # CHECK: {{^{ level 1$}}
    # CHECK: {{^    \{$}}
    # CHECK: {{^        level 2$}}
    # CHECK: {{^    \}$}}
    # CHECK: {{^\}$}}
} | $fish_indent

echo 'test 1 -eq 1; or {
    echo a
    echo b
}' | $fish_indent
# CHECK: test 1 -eq 1; or {
# CHECK: {{^    }}echo a
# CHECK: {{^    }}echo b
# CHECK: {{^}}{{[}]}}

echo 'not {
    echo hi
}' | $fish_indent
# CHECK: not {
# CHECK: {{^    }}echo hi
# CHECK: {{^}}{{[}]}}

echo 'time {
    echo hi
}' | $fish_indent
# CHECK: time {
# CHECK: {{^    }}echo hi
# CHECK: {{^}}{{[}]}}

echo 'echo x{a,
  b}y' | $fish_indent
# CHECK: echo x{a,
# CHECK: {{^  }}b}y

echo 'multiline-\\
-word' | $fish_indent --check
echo $status #CHECK: 0

echo 'PATH={$PATH[echo " "' | $fish_indent --ansi
# CHECK: PATH={$PATH[echo " "

fish_config theme choose "ayu Dark"
echo -n 'echo hello' | builtin fish_indent --ansi
echo end
# CHECK: {{\x1b\[38;2;57;186;230mecho\x1b\[38;2;179;177;173m hello\x1b\[38;2;242;150;104m\x1b\[m}}
# CHECK: end

echo a\> | $fish_indent
# CHECK: a >

echo a\<\) | $fish_indent
# CHECK: a < )
echo b\|\{ | $fish_indent
# CHECK: b | {

echo "\'\\\\\x00\'" | string unescape | $fish_indent | string escape
# CHECK: \'\\\x00\'

echo '\"\"\|\x00' | string unescape | $fish_indent | string unescape
# CHECK: |

echo 'a


;


b
' | $fish_indent
#CHECK: a
#CHECK:
#CHECK: b

echo "



echo this file starts late
" | $fish_indent
#CHECK: echo this file starts late

echo 'foo|bar; begin
echo' | $fish_indent --only-indent
# CHECK: foo|bar; begin
# CHECK: {{^}}    echo

echo 'begin
    echo
end' | $fish_indent --only-unindent
# CHECK: {{^}}begin
# CHECK: {{^}}echo
# CHECK: {{^}}end

echo 'if true
    begin
        echo
    end
end' | $fish_indent --only-unindent
# CHECK: {{^}}if true
# CHECK: {{^}}begin
# CHECK: {{^}}echo
# CHECK: {{^}}end
# CHECK: {{^}}end

echo 'begin
    echo
  not indented properly
end' | $fish_indent --only-unindent
# CHECK: {{^}}begin
# CHECK: {{^}}    echo
# CHECK: {{^}}  not indented properly
# CHECK: {{^}}end


echo 'echo (
if true
echo
end
)' | $fish_indent --only-indent
# CHECK: {{^}}echo (
# CHECK: {{^}}    if true
# CHECK: {{^}}        echo
# CHECK: {{^}}    end
# CHECK: {{^}})

echo 'echo (
if true
echo "
multi
line
"
end
)' | $fish_indent --only-indent
# CHECK: {{^}}echo (
# CHECK: {{^}}    if true
# CHECK: {{^}}        echo "
# CHECK: {{^}}multi
# CHECK: {{^}}line
# CHECK: {{^}}"
# CHECK: {{^}}    end
# CHECK: {{^}})

echo 'echo (
if true
echo "
multi
line
"
end
)' | builtin fish_indent --only-indent
# CHECK: {{^}}echo (
# CHECK: {{^}}    if true
# CHECK: {{^}}        echo "
# CHECK: {{^}}multi
# CHECK: {{^}}line
# CHECK: {{^}}"
# CHECK: {{^}}    end
# CHECK: {{^}})

set -l tmpdir (mktemp -d)
echo 'echo "foo" "bar"' > $tmpdir/indent_test.fish
$fish_indent --write $tmpdir/indent_test.fish
cat $tmpdir/indent_test.fish
# CHECK: echo foo bar

# See that the builtin can be redirected
printf %s\n a b c | builtin fish_indent | grep b
# CHECK: b
