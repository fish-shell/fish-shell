#RUN: fish=%fish %fish %s

# Verify cd error messages strip control characters from the path argument.
# Hostile-named directories must not be able to inject terminal sequences
# via the "directory does not exist" error.

cd (printf '/tmp/__nope_a\x1bb\x07c\x9dd')
# CHECKERR: cd: The directory '/tmp/__nope_abcd' does not exist
# CHECKERR: {{.*}}cd.fish (line {{\d+}}):
# CHECKERR: builtin cd $argv
# CHECKERR: ^
# CHECKERR: in function 'cd' with arguments {{.*}}
# CHECKERR: {{.*}}called on line {{\d+}} of file {{.*}}
