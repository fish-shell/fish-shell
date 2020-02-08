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

# basic expansion test
echo {}
echo {apple}
echo {apple,orange}
#CHECK: {}
#CHECK: {apple}
#CHECK: apple orange

# expansion tests with spaces
echo {apple, orange}
echo { apple, orange, banana }
#CHECK: apple orange
#CHECK: apple orange banana

# expansion with spaces and cartesian products
echo \'{ hello , world }\'
#CHECK: 'hello' 'world'

# expansion with escapes
for phrase in {good\,,   beautiful ,morning}; echo -n "$phrase "; end | string trim;
for phrase in {goodbye\,,\ cruel\ ,world\n}; echo -n $phrase; end;
#CHECK: good, beautiful morning
#CHECK: goodbye, cruel world

# dual expansion cartesian product
echo { alpha, beta }\ {lambda, gamma }, |  string replace -r ',$' ''
#CHECK: alpha lambda, beta lambda, alpha gamma, beta gamma

# expansion with subshells
for name in { (echo Meg), (echo Jo) }
	echo $name
end
#CHECK: Meg
#CHECK: Jo

# subshells with expansion
for name in (for name in {Beth, Amy}; printf "$name\n"; end); printf "$name\n"; end
#CHECK: Beth
#CHECK: Amy

echo {{a,b}}
#CHECK: {a} {b}
