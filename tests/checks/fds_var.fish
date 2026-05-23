# RUN: fish=%fish %fish -C "set -x helper %fish_test_helper" %s
# REQUIRES: command -v %fish_test_helper

set -l fds ($helper print_fds)
test "$fds" = "0 1 2"
or begin
    echo This test needs to have all fds other than 0 1 and 2 closed
    echo Please close the other fds and try again
    exit 1
end

cd $TMPDIR

# Run in a sub-shell so we don't pollute stderr with the flog warnings.
# We'll check those warnings at the end
if test "$argv" = subshell
    # Basic write
    set --fd=wo write_fd ./file1
    echo foobar >&$write_fd
    cat ./file1
    # CHECK: foobar
    $helper print_fds
    # CHECK: 0 1 2 {{\d\d}}

    # Basic deletion
    set -e --fd write_fd
    $helper print_fds
    # CHECK: 0 1 2

    # Basic append
    set --fd=wo,append write_fd ./file1
    echo barfoo >&$write_fd
    cat ./file1
    set -e --fd write_fd
    # CHECK: foobar
    # CHECK: barfoo

    # `append` as a shortcut for `wo,append`
    set --fd=append write_fd ./file1
    echo foofoo >&$write_fd
    cat ./file1
    set -e --fd write_fd
    # CHECK: foobar
    # CHECK: barfoo
    # CHECK: foofoo

    # Basic read
    set --fd=ro read_fd ./file1
    cat <&$read_fd
    # CHECK: foobar
    # CHECK: barfoo
    # CHECK: foofoo
    set -e --fd read_fd

    # Write clobbering
    set --fd=wo,try clobber_fd ./file1
    test -z "$clobber_fd" && echo good || echo bad
    # CHECK: good
    # with flog output, checked at the bottom (no-clobber)

    # Read try on existing file
    echo exists >./exists
    set --fd=ro,try try_fd ./exists
    cat <&$try_fd
    # CHECK: exists
    echo exists-$status
    # CHECK: exists-0
    set -e --fd try_fd

    # Read try on non-existent file
    set --fd=ro,try null_fd ./non-existent
    cat <&$null_fd
    # no error about invalid descriptor
    echo non-existent-$status
    # CHECK: non-existent-0
    test -e ./non-existent && echo bad || echo good
    # CHECK: good
    set -e --fd null_fd

    # Duplicates mode args are ok
    set --fd=wo,wo,append,append var ./file1
    set -e --fd var
    # extra commas are ok
    set --fd=wo,,,append,, var ./file1
    set -e --fd var

    # Invalid arguments and combos
    set --fd var ./file2
    # CHECKERR: set: --fd: option requires an argument when setting a variable
    # CHECKERR: {{.*}}/fds_var.fish (line {{\d+}}):
    # CHECKERR: set --fd var ./file2
    # CHECKERR: ^
    # CHECKERR: (Type 'help set' for related documentation)
    set --fd=ro,wo var ./file2
    # CHECKERR: set: --fd: option arguments `ro` and `wo` cannot be used together
    # CHECKERR: {{.*}}/fds_var.fish (line {{\d+}}):
    # CHECKERR: set --fd=ro,wo var ./file2
    # CHECKERR: ^
    # CHECKERR: (Type 'help set' for related documentation)
    set --fd=ro,append var ./file2
    # CHECKERR: set: --fd: option arguments `ro` and `append` cannot be used together
    # CHECKERR: {{.*}}/fds_var.fish (line {{\d+}}):
    # CHECKERR: set --fd=ro,append var ./file2
    # CHECKERR: ^
    # CHECKERR: (Type 'help set' for related documentation)
    set --fd=try,append var ./file2
    # CHECKERR: set: --fd: option arguments `try` and `append` cannot be used together
    # CHECKERR: {{.*}}/fds_var.fish (line {{\d+}}):
    # CHECKERR: set --fd=try,append var ./file2
    # CHECKERR: ^
    # CHECKERR: (Type 'help set' for related documentation)
    set --fd=foo,invalid,val var ./file2
    # CHECKERR: set: --fd: invalid option argument(s): `foo`, `invalid`, `val`
    # CHECKERR: {{.*}}/fds_var.fish (line {{\d+}}):
    # CHECKERR: set --fd=foo,invalid,val var ./file2
    # CHECKERR: ^
    # CHECKERR: (Type 'help set' for related documentation)
    set -e --fd= var
    # CHECKERR: set: --fd: option does not take an argument when erasing a variable
    # CHECKERR: {{.*}}/fds_var.fish (line {{\d+}}):
    # CHECKERR: set -e --fd= var
    # CHECKERR: ^
    # CHECKERR: (Type 'help set' for related documentation)
    set -S --fd var
    # CHECKERR: set: invalid option combination
    # CHECKERR: {{.*}}/fds_var.fish (line {{\d+}}):
    # CHECKERR: set -S --fd var
    # CHECKERR: ^
    # CHECKERR: (Type 'help set' for related documentation)
    set -S --fd=ro var
    # CHECKERR: set: invalid option combination
    # CHECKERR: {{.*}}/fds_var.fish (line {{\d+}}):
    # CHECKERR: set -S --fd=ro var
    # CHECKERR: ^
    # CHECKERR: (Type 'help set' for related documentation)
    set -S --fd var
    # CHECKERR: set: invalid option combination
    # CHECKERR: {{.*}}/fds_var.fish (line {{\d+}}):
    # CHECKERR: set -S --fd var
    # CHECKERR: ^
    # CHECKERR: (Type 'help set' for related documentation)
    set -q --fd var
    # CHECKERR: set: invalid option combination
    # CHECKERR: {{.*}}/fds_var.fish (line {{\d+}}):
    # CHECKERR: set -q --fd var
    # CHECKERR: ^
    # CHECKERR: (Type 'help set' for related documentation)
    set -q --fd=ro var
    # CHECKERR: set: invalid option combination
    # CHECKERR: {{.*}}/fds_var.fish (line {{\d+}}):
    # CHECKERR: set -q --fd=ro var
    # CHECKERR: ^
    # CHECKERR: (Type 'help set' for related documentation)
    set -n --fd var
    # CHECKERR: set: invalid option combination
    # CHECKERR: {{.*}}/fds_var.fish (line {{\d+}}):
    # CHECKERR: set -n --fd var
    # CHECKERR: ^
    # CHECKERR: (Type 'help set' for related documentation)
    set -n --fd=ro var
    # CHECKERR: set: invalid option combination
    # CHECKERR: {{.*}}/fds_var.fish (line {{\d+}}):
    # CHECKERR: set -n --fd=ro var
    # CHECKERR: ^
    # CHECKERR: (Type 'help set' for related documentation)

    # Should fail for having a value, not for having an invalid value
    set -S --fd=invalid_val var ./file
    # CHECKERR: set: invalid option combination
    # CHECKERR: {{.*}}/fds_var.fish (line {{\d+}}):
    # CHECKERR: set -S --fd=invalid_val var ./file
    # CHECKERR: ^
    # CHECKERR: (Type 'help set' for related documentation)
    set -S --fd=invalid_val var ./file
    # CHECKERR: set: invalid option combination
    # CHECKERR: {{.*}}/fds_var.fish (line {{\d+}}):
    # CHECKERR: set -S --fd=invalid_val var ./file
    # CHECKERR: ^
    # CHECKERR: (Type 'help set' for related documentation)
    set -q --fd=invalid_val var ./file
    # CHECKERR: set: invalid option combination
    # CHECKERR: {{.*}}/fds_var.fish (line {{\d+}}):
    # CHECKERR: set -q --fd=invalid_val var ./file
    # CHECKERR: ^
    # CHECKERR: (Type 'help set' for related documentation)
    set -n --fd=invalid_val var ./file
    # CHECKERR: set: invalid option combination
    # CHECKERR: {{.*}}/fds_var.fish (line {{\d+}}):
    # CHECKERR: set -n --fd=invalid_val var ./file
    # CHECKERR: ^
    # CHECKERR: (Type 'help set' for related documentation)

    # Create array of fds...
    set --fd=wo var ./file1 ./file2 ./file3
    set -S var
    # CHECK: $var: set in global scope, unexported, with 3 elements
    # CHECK: $var[1]: |{{\d\d}}|
    # CHECK: $var[2]: |{{\d\d}}|
    # CHECK: $var[3]: |{{\d\d}}|
    $helper print_fds
    # CHECK: 0 1 2 {{\d\d}} {{\d\d}} {{\d\d}}
    # ... and delete some
    set -e --fd var[2]
    set -S var
    # CHECK: $var: set in global scope, unexported, with 2 elements
    # CHECK: $var[1]: |{{\d\d}}|
    # CHECK: $var[2]: |{{\d\d}}|
    $helper print_fds
    # CHECK: 0 1 2 {{\d\d}} {{\d\d}}
    set -e --fd var
    $helper print_fds
    # CHECK: 0 1 2

    # Leaks on deletion
    set --fd=wo var ./file1
    set var_tmp $var
    set -e var
    set -S var
    $helper print_fds
    # CHECK: 0 1 2 {{\d\d}}
    set -e --fd var_tmp
    $helper print_fds
    # CHECK: 0 1 2

    # Leaks on re-assignment
    set --fd=wo var ./file1
    set var_tmp $var
    set --fd=wo var ./file2
    echo in_file2 >&$var
    cat ./file2
    # CHECK: in_file2
    $helper print_fds
    # CHECK: 0 1 2 {{\d\d}} {{\d\d}}
    set -e --fd var
    $helper print_fds
    # CHECK: 0 1 2 {{\d\d}}
    set -e --fd var_tmp

    # Append/Prepend
    set var mark
    set -S var
    # CHECK: $var: set in global scope, unexported, with 1 elements
    # CHECK: $var[1]: |mark|
    set --prepend --fd=wo var ./file1
    set -S var
    # CHECK: $var: set in global scope, unexported, with 2 elements
    # CHECK: $var[1]: |{{\d\d}}|
    # CHECK: $var[2]: |mark|
    set --append --fd=wo var ./file2
    set -S var
    # CHECK: $var: set in global scope, unexported, with 3 elements
    # CHECK: $var[1]: |{{\d\d}}|
    # CHECK: $var[2]: |mark|
    # CHECK: $var[3]: |{{\d\d}}|
    set -e --fd var

    # Invalid file
    set --fd=wo var ""
    # with flog output, checked at the bottom (empty-path)
    set --fd=wo var ./no-dir/file
    # with flog output, checked at the bottom (no-write-dir)

    # Duplicate file fd
    set --fd=wo var ./file1
    echo -n foo >&$var
    set --append --fd=wo var $var
    echo bar >&$var[2]
    cat ./file1
    # CHECK: foobar
    test $var[1] -ne $var[2] && echo good || echo bad
    # CHECK: good
    set -e --fd var

    # Duplicate standard fd
    set --fd=wo var 1
    $helper print_fds
    # CHECK: 0 1 2 {{\d\d}}
    echo foobar >&$var
    # CHECK: foobar
    set -e --fd var

    # Negative numbers are considered paths
    set --fd=wo var -1
    echo foobar >&$var
    cat -- ./-1
    # CHECK: foobar
    set -e --fd var

    touch ./file1 ./file2 ./file3
    set --fd=ro var ./file1 ./file2 ./bad_file ./file3
    # with flog output, checked at the bottom (array-with-bad-file)
    set -S var
    # CHECK: $var: set in global scope, unexported, with 4 elements
    # CHECK: $var[1]: |{{\d\d}}|
    # CHECK: $var[2]: |{{\d\d}}|
    # CHECK: $var[3]: ||
    # CHECK: $var[4]: |{{\d\d}}|
    set -e --fd var

    # Erasing an array with a mix of strings and fds
    # (no leaks, nor "invalid descriptor" messages, unless the value looks like a descriptor)
    set var str1
    set --append --fd=wo var ./file1
    set --append var str2
    set --append --fd=wo var ./file2
    set --append var str3
    set --append var 55
    set -e --fd var
    $helper print_fds
    # CHECK: 0 1 2
    # with flog output, checked at the bottom (close-fake-descriptor)

    # CLOEXEC
    set --fd=wo var ./file1
    set --append --fd=wo,cloexec var ./file2
    $helper print_fds
    # We only see one fd, since the other is cloexec...
    # CHECK: 0 1 2 {{\d\d}}
    echo "in file1 direct" >&$var[1]
    echo "in file2 direct" >&$var[2]
    $fish -c "echo 'in file1 via subshell' >&$var[1]"
    $fish -c "echo 'in file2 via subshell' >&$var[2]"
    cat ./file1 ./file2
    # CHECK: in file1 direct
    # CHECK: in file1 via subshell
    # CHECK: in file2 direct
    if __fish_is_cygwin || test "$(uname -s)" = Darwin
        # The test on Cygwin and MacOS does not print the error message
        # while it works on Linux, or on Cygwin when running the commands manually
        echo "write: bla" >&2
    end
    # CHECKERR: write: {{.+}}
    set -e --fd var

    # Check --reuse ...
    # ... create an fd number to reuse ...
    set --fd=wo var ./file1
    set saved_fd_number $var
    # ... `--reuse` doesn't work on whole variable ...
    set --fd=wo,reuse var ./file2
    # CHECKERR: set: --fd=reuse: can only be used when assigning to a slice
    # CHECKERR: {{.*}}/fds_var.fish (line {{\d+}}):
    # CHECKERR: set --fd=wo,reuse var ./file2
    # CHECKERR: ^
    # CHECKERR: (Type 'help set' for related documentation)
    # ... but works on slices, keeps the same fd number ...
    set --fd=wo,reuse var[1] ./file2
    test $saved_fd_number -eq $var && echo good || echo bad
    # CHECK: good
    # ... but pointing to the new target
    echo "should be in file2" >&$var[1]
    cat ./file2
    # CHECK: should be in file2
    set -e --fd var

    # Use RO fd for writing
    set --fd=ro var ./file1
    echo foo >&$var
    # CHECKERR: write: {{.+}}
    set -e --fd var

    # Use WO fd for reading
    set --fd=wo write_fd ./file1
    cat <&$write_fd
    # CHECKERR: cat: {{-|stdin}}: {{.+}}
    set -e --fd write_fd
    set -e --fd read_fd

    # There shouldn't be any stray fd
    $helper print_fds
    # CHECK: 0 1 2

else
    set -l debug_file "$TMPDIR/debug.log"
    $fish --debug-output=$debug_file (status --current-filename) subshell
    cat $debug_file >&2
    # no-clobber
    # CHECKERR: warning: `./file1`: {{.+}}

    # empty-path
    # CHECKERR: warning: Failed to open ``: {{.+}}

    # no-write-dir
    # CHECKERR: warning: Failed to open `./no-dir/file`: {{.+}}

    # array-with-bad-file
    # CHECKERR: warning: Failed to open `./bad_file`: {{.+}}

    # close-fake-descriptor
    # CHECKERR: warning: Failed to close descriptor `var[6]` (55): {{.+}}
end
