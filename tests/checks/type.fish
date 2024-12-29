# RUN: %fish %s
#
# Tests for the `type` builtin
# First type --query, which is the most important part.
type -q type
echo $status
# CHECK: 0
type -q realpath
echo $status
# CHECK: 0
type -q sh
echo $status
# CHECK: 0
type -q doesnotexist-pleasedonotexist-2324242
echo $status
# CHECK: 1
type -q doesnotexist-pleasedonotexist-2324242 sh
echo $status
# CHECK: 0
type -q sh doesnotexist-pleasedonotexist-2324242
echo $status
# CHECK: 0
type -q '['
echo $status
# CHECK: 0
# Confirm that --quiet is still a thing
type --quiet '['
echo $status
# CHECK: 0

# Test that we print a command path
type sh
# (we resolve the path, so if /bin is a symlink to /usr/bin this shows /usr/bin/sh)
# CHECK: sh is {{.*}}/sh

# Test that we print a function definition.
# The exact definition and description here depends on the system, so we'll ignore the actual code.
type realpath | grep -v "^  *"
# CHECK: realpath is a function with definition
# CHECK: # Defined in {{.*}}/functions/realpath.fish @ line {{\d+}}
# CHECK: function realpath --description {{.+}}
# CHECK: end

type -t realpath foobar
# CHECK: function
# CHECKERR: type: Could not find {{.}}foobar{{.}}

type -P test
# CHECK: {{/.*}}test
type -P ls
# CHECK: {{/.*}}ls

type
echo $status
# CHECK: 1
type -q
echo $status
# CHECK: 1

type -p alias
# CHECK: {{.*}}/alias.fish

type -s alias
# CHECK: alias is a function (Defined in {{.*}}/alias.fish @ line {{\d+}})

function test-type
    echo this is a type test
end

type test-type
# CHECK: test-type is a function with definition
# CHECK: # Defined in {{.*}}/type.fish @ line {{\d+}}
# CHECK: function test-type
# CHECK: echo this is a type test
# CHECK: end

type -p test-type
# CHECK: {{.*}}/type.fish

functions -c test-type test-type2
type test-type2
# CHECK: test-type2 is a function with definition
# CHECK: # Defined in {{.*}}/type.fish @ line {{\d+}}, copied in {{.*}}/type.fish @ line {{\d+}}
# CHECK: function test-type2
# CHECK: echo this is a type test
# CHECK: end

type -p test-type2
# CHECK: {{.*}}/type.fish

type -s test-type2
# CHECK: test-type2 is a function (Defined in {{.*}}/type.fish @ line {{\d+}}, copied in {{.*}}/type.fish @ line {{\d+}})

echo "functions -c test-type test-type3" | source
type test-type3
# CHECK: test-type3 is a function with definition
# CHECK: # Defined in {{.*}}/type.fish @ line {{\d+}}, copied via `source`
# CHECK: function test-type3
# CHECK: echo this is a type test
# CHECK: end

type -p test-type3
# CHECK: -

type -s test-type3
# CHECK: test-type3 is a function (Defined in {{.*}}/type.fish @ line {{\d+}}, copied via `source`)

echo "function other-test-type; echo this is a type test; end" | source

functions -c other-test-type other-test-type2
type other-test-type2
# CHECK: other-test-type2 is a function with definition
# CHECK: # Defined via `source`, copied in {{.*}}/type.fish @ line {{\d+}}
# CHECK: function other-test-type2
# CHECK: echo this is a type test;
# CHECK: end

type -p other-test-type2
# CHECK: {{.*}}/type.fish

type -s other-test-type2
# CHECK: other-test-type2 is a function (Defined via `source`, copied in {{.*}}/type.fish @ line {{\d+}})

echo "functions -c other-test-type other-test-type3" | source
type other-test-type3
# CHECK: other-test-type3 is a function with definition
# CHECK: # Defined via `source`, copied via `source`
# CHECK: function other-test-type3
# CHECK: echo this is a type test;
# CHECK: end

type -p other-test-type3
# CHECK: -

type -s other-test-type3
# CHECK: other-test-type3 is a function (Defined via `source`, copied via `source`)

touch ./test
chmod +x ./test

PATH=.:$PATH type -P test
# CHECK: ./test
