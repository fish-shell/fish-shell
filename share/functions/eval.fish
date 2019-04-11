# This empty function is required as while `eval` is implemented internally as
# a decorator, unlike other decorators such as `command` or `builtin`, it is
# *not* stripped from the statement by the parser before execution to internal
# quirks in how statements are handled; the presence of this function allows
# constructs such as command substitution to be used in the head of an
# evaluated statement.
#
# The function defined below is only executed if a bare `eval` is executed (with
# no arguments), in all other cases it is bypassed entirely.

function eval
end
