# RUN: %fish %s
# Test redirecting builtin help with a pipe
# REQUIRES: command -v mandoc || command -v nroff

set -lx __fish_data_dir (mktemp -d)
mkdir -p $__fish_data_dir/man/man1
# Create $__fish_data_dir/man/man1/and.1
echo '.\" Test manpage for and (not real).
.TH "AND" "1" "Feb 02, 2024" "3.7" "fish-shell"
.SH NAME
and \- conditionally execute a command' >$__fish_data_dir/man/man1/and.1

# help should be redirected to grep instead of appearing on STDOUT
builtin and --help | grep -q "and - conditionally execute a command"
echo $status
#CHECK: 0
