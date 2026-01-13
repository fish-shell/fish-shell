# RUN: %fish %s
#
# These lines left around because we need the line numbers.
# This file in general requires careful editing in the middle, I recommend appending.
function t --on-event linenumber
    status line-number
    status line-number
    status line-number
end

emit linenumber
# CHECK: 6
# CHECK: 7
# CHECK: 8

type --nonexistent-option-so-we-get-a-backtrace
# CHECKERR: type: --nonexistent-option-so-we-get-a-backtrace: unknown option

type --short=cd
# CHECKERR: type: --short=cd: option does not take an argument

function line-number
    status line-number
end

line-number
# CHECK: 23

bind -M
# CHECKERR: bind: -M: option requires an argument
# CHECKERR: {{.*}} (line 29):
# CHECKERR: bind -M
# CHECKERR: ^
# CHECKERR: (Type 'help bind' for related documentation)
