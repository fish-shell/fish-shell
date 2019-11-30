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

alias foo '"a b" c d e'
# Bare `alias` should list the aliases we have created and nothing else
# We have to exclude two aliases because they're an artifact of the unit test
# framework and we can't predict the definition.
alias | grep -Ev '^alias (fish_indent|fish_key_reader) '
# CHECK: alias a-2 'echo "hello there"'
# CHECK: alias foo '"a b" c d e'
# CHECK: alias my_alias 'foo; and echo foo ran'
