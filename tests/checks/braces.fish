#RUN: %ghoti %s

echo x-{1}
#CHECK: x-{1}

echo x-{1,2}
#CHECK: x-1 x-2

echo foo-{1,2{3,4}}
#CHECK: foo-1 foo-23 foo-24

echo foo-{} # literal "{}" expands to itself
#CHECK: foo-{}

echo foo-{{},{}} # the inner "{}" expand to themselves, the outer pair expands normally.
#CHECK: foo-{} foo-{}

echo foo-{{a},{}} # also works with something in the braces.
#CHECK: foo-{a} foo-{}

echo foo-{""} # still expands to foo-{}
#CHECK: foo-{}

echo foo-{$undefinedvar} # still expands to nothing
#CHECK: 

echo foo-{,,,} # four empty items in the braces.
#CHECK: foo- foo- foo- foo-

echo foo-{,\,,} # an empty item, a "," and an empty item.
#CHECK: foo- foo-, foo-

echo .{  foo  bar  }. # see 6564
#CHECK: .{  foo  bar  }.

# whitespace within entries is retained
for foo in {a, hello
wo  rld }
    echo \'$foo\'
end
# CHECK: 'a'
# CHECK: 'hello
# CHECK: wo  rld'

for foo in {hello
world}
    echo \'$foo\'
end
#CHECK: '{hello
#CHECK: world}'
