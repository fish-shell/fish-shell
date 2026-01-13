# RUN: %fish %s
# Regression test for issue #4443
eval set -l previously_undefined foo
echo $previously_undefined
# CHECK: foo

# Test redirection
eval "echo you can\\'t see this 1>&2" 2>/dev/null

# Test return statuses
false
eval true
echo $status
# CHECK: 0
false
eval false
echo $status
# CHECK: 1

# Test return status in case of parsing error
false
eval "("
echo $status
# CHECK: 123
# CHECKERR: {{.*}}checks/eval.fish (line {{\d+}}): Unexpected end of string, expecting ')'
# CHECKERR: (
# CHECKERR: ^
false
eval '""'
echo $status
# CHECK: 123
# CHECKERR: {{.*}}checks/eval.fish (line {{\d+}}): The expanded command was empty.
# CHECKERR: ""
# CHECKERR: ^^

function empty
end

# Regression tests for issue #5692
false
eval
echo blank eval: $status # 0
# CHECK: blank eval: 0

false
eval ""
echo empty arg eval: $status # 0
# CHECK: empty arg eval: 0

false
eval empty
echo empty function eval $status # 0
# CHECK: empty function eval 0

false
eval "begin; end;"
echo empty block eval: $status # 0
# CHECK: empty block eval: 0

source /banana/\t/foo
# CHECKERR: source: Error encountered while sourcing file '/banana/\t/foo':
# CHECKERR: source: No such file or directory

# See that eval can't be defined as a function
function eval
    builtin eval $argv
end
# CHECKERR: {{.*}}checks/eval.fish (line {{\d+}}): function: eval: cannot use reserved keyword as function name
# CHECKERR: function eval
# CHECKERR: ^~~~~~~~~~~~^


function evil --no-scope-shadowing
    eval $argv
end

# And this is why we do this: `eval` *can't* be cleanly shadowed with a function,
# because that would always introduce a new block or at the very least clobber $argv.
eval set -l argv this works
echo $argv
# CHECK: this works
evil set -l argv this does not
echo $argv
# CHECK: this works
