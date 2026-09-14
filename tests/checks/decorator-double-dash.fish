# RUN: fish=%fish fish_indent=%fish_indent %fish %s

command -- printf '%s\n' command
# CHECK: command
builtin -- echo builtin
# CHECK: builtin
$fish -c 'exec -- printf "%s\n" exec'
# CHECK: exec

# A separator protects a command name starting with a dash.
printf '#!/bin/sh\necho dash-command\n' >./-q
chmod +x ./-q
PATH="$PWD:$PATH" command -- -q
# CHECK: dash-command

printf '%s\n' 'command -- -q' 'builtin -- echo hello' 'exec -- -q' 'time -- -q' | $fish_indent
# CHECK: command -- -q
# CHECK: builtin -- echo hello
# CHECK: exec -- -q
# CHECK: time -- -q

time -- { echo hello }
# CHECK: hello
# CHECKERR: ___{{.*}}
# CHECKERR: {{.*}}
# CHECKERR: {{.*}}
# CHECKERR: {{.*}}

not time -- { false }
echo $status
# CHECK: 0
# CHECKERR: ___{{.*}}
# CHECKERR: {{.*}}
# CHECKERR: {{.*}}
# CHECKERR: {{.*}}
