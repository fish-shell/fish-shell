# RUN: %fish %s

builtin fg -- -1
# CHECKERR: fg: '-1' is not a valid process ID
# CHECKERR: {{.*}}/fg.fish (line {{\d+}}):
# CHECKERR: builtin fg -- -1
# CHECKERR: ^
# CHECKERR:
# CHECKERR: (Type 'help fg' for related documentation)
