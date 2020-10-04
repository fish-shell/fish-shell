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

# Test that we print a command path
type sh
# (we resolve the path, so if /bin is a symlink to /usr/bin this shows /usr/bin/sh)
# CHECK: sh is {{.*}}/bin/sh

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
