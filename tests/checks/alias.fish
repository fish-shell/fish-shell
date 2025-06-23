#RUN: %fish %s
# Avoid regressions of issue \#3860 wherein the first word of the alias ends with a semicolon
function foo
    echo ran foo
end
alias my_alias "foo; and echo foo ran"
my_alias
# CHECK: ran foo
# CHECK: foo ran

alias a-2='echo "hello there"'
alias a-3='echo hello\ there'

alias foo '"a b" c d e'
# Bare `alias` should list the aliases we have created and nothing else
# We have to exclude two aliases because they're an artifact of the unit test
# framework and we can't predict the definition.
alias | grep -Ev '^alias (fish_indent|fish_key_reader) '
# CHECK: alias a-2 'echo "hello there"'
# CHECK: alias a-3 'echo hello\\\\ there'
# CHECK: alias foo '"a b" c d e'
# CHECK: alias my_alias 'foo; and echo foo ran'

# #4756 - missing "--" argument to string causing issues with "--" options
alias l. "ls -d .*"
# No output

alias d "'/mnt/c/Program Files (x86)/devenv.exe' /Edit"
functions d | sed s,^,^,
# CHECK: ^# Defined via `source`
# CHECK: ^function d --wraps="'/mnt/c/Program Files (x86)/devenv.exe' /Edit" --description "alias d '/mnt/c/Program Files (x86)/devenv.exe' /Edit"
# CHECK: ^    '/mnt/c/Program Files (x86)/devenv.exe' /Edit $argv
# CHECK: ^end

# Use "command" to prevent recursion, and don't add --wraps to avoid accidental recursion in completion.
alias e 'e --option=value'
functions e
# CHECK: # Defined via `source`
# CHECK: function e --description 'alias e e --option=value'
# CHECK: command e --option=value $argv
# CHECK: end

# Don't add --wraps if it looks like a wrapper command to avoid accidental recursion in completion.
alias f 'wrapper around f'
functions f
# CHECK: # Defined via `source`
# CHECK: function f --description 'alias f wrapper around f'
# CHECK: wrapper around f $argv
# CHECK: end
