# RUN: %fish %s
# REQUIRES: command -v man

# Test redirecting builtin help with a pipe
# help should be redirected to grep instead of appearing on STDOUT
builtin and --help 2>| grep -q "Documentation for and"
echo $status
#CHECK: 0

function __fish_print_help
    return 2
end
builtin and --help
# CHECKERR: and: missing man page
# CHECKERR: Documentation may not be installed.
# CHECKERR: `help and` will show an online version
