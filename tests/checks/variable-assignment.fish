# RUN: %fish %s

# erase all lowercase variables to make sure they don't break our tests
for varname in (set -xn | string match -r '^[a-z].*')
    while set -q $varname
        set -e $varname
    end
end

# CHECK: bar
foo=bar echo $foo

# CHECK: nil
set -q foo; or echo nil

# CHECK: lx
foo=bar set -qlx foo; and echo lx

# CHECK: 3
a={1, 2, 3} count $a

# CHECK: 1+2+3
a={1, 2, 3} string join + $a

# CHECK: 1 2 3
a=(echo 1 2 3) echo $a

# CHECK: a a2
a=a b={$a}2 echo $a $b

# CHECK: a
a=a builtin echo $a

# CHECK: 0
a=failing-glob-* count $a

# CHECK: ''
a=b true | echo "'$a'"

if a=b true
    # CHECK: ''
    echo "'$a'"
end

# CHECK: b
not a=b echo $a

# CHECK: b
a=b not echo $a

# CHECK: b
a=b not builtin echo $a

# CHECK: /usr/bin:/bin
xPATH={/usr,}/bin sh -c 'echo $xPATH'

# CHECK: 2
yPATH=/usr/bin:/bin count $yPATH

# CHECK: b
a=b begin
    true | echo $a
end

# CHECK: b
a=b if true
    echo $a
end

# CHECK: b
a=b switch x
    case x
        echo $a
end

complete -c x --erase
complete -c x -xa arg
complete -C' a=b x ' # Mind the leading space.
# CHECK: arg
alias xalias=x
complete -C'a=b xalias '
# CHECK: arg
alias envxalias='a=b x'
complete -C'a=b envxalias '
# CHECK: arg

# Eval invalid grammar to allow fish to parse this file
a=(echo b1)
echo a is $a
# CHECK: a is b1

# FIXME
# : | a=b2
# echo a is $a
# # CHECK: echo a is b2

# TODO(posix_mode)
# not a=b3
# echo a is $a
# # CHECK: echo a is b3

complete -c foo -xa '$a'
a=b complete -C'foo '
#CHECK: b
