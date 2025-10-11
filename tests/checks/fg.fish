# RUN: %fish %s

fg (math 2 ^ 31 - 1)
# CHECKERR: fg: No suitable job: 2147483647

fg (math 2 ^ 31)
# CHECKERR: fg: '2147483648' is not a valid process ID
# CHECKERR: {{.*}}config.fish (line {{\d+}}):
# CHECKERR:     and builtin fg $args[-1]
# CHECKERR:         ^
# CHECKERR: in function 'fg' with arguments '2147483648'
# CHECKERR: {{\t}}called on line {{\d+}} of file {{.*}}/fg.fish
# CHECKERR: (Type 'help fg' for related documentation)

fg 0 2>| string match --max-matches=1 '*' >&2
# CHECKERR: fg: '0' is not a valid process ID

builtin fg -- -1 2>| string match --max-matches=1 '*' >&2
# CHECKERR: fg: '-1' is not a valid process ID

builtin fg -- -1 2>| string match --max-matches=1 '*' >&2
# CHECKERR: fg: '-1' is not a valid process ID

builtin fg -- -(math 2 ^ 31) 2>| string match --max-matches=1 '*' >&2
# CHECKERR: fg: '-2147483648' is not a valid process ID
