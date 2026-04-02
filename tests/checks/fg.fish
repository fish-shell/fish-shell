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

fg
# CHECKERR: fg: There are no suitable jobs

builtin fg -- -1 2>| string match --max-matches=1 '*' >&2
# CHECKERR: fg: '-1' is not a valid process ID

builtin fg -- -1 2>| string match --max-matches=1 '*' >&2
# CHECKERR: fg: '-1' is not a valid process ID

builtin fg -- -(math 2 ^ 31) 2>| string match --max-matches=1 '*' >&2
# CHECKERR: fg: '-2147483648' is not a valid process ID

builtin fg 1 2
# CHECKERR: fg: '1' is not a job
# CHECKERR: {{.*}}/fg.fish (line {{\d+}}):
# CHECKERR: builtin fg 1 2
# CHECKERR: ^
# CHECKERR: (Type 'help fg' for related documentation)

sleep 1 &
sleep 1 &
builtin fg (jobs --pid)
# CHECKERR: fg: Ambiguous job
# CHECKERR: {{.*}}/fg.fish (line {{\d+}}):
# CHECKERR: builtin fg (jobs --pid)
# CHECKERR: ^
# CHECKERR: (Type 'help fg' for related documentation)
set -l pid (jobs -lp)
fg $pid
# CHECKERR: fg: Can't put job {{\d+}}, 'sleep 1 &' to foreground because it is not under job control
