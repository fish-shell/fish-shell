# RUN: %fish -C "set helper %fish_test_helper" %s

# Check that we don't leave stray FDs.

$helper print_fds
# CHECK: 0 1 2

$helper print_fds 0>&-
# CHECK: 1 2

$helper print_fds 0>&- 2>&-
# CHECK: 1

false | $helper print_fds 0>&-
# CHECK: 0 1 2

$helper print_fds <(status current-filename)
# CHECK: 0 1 2

$helper print_fds <(status current-filename)
# CHECK: 0 1 2

$helper print_fds 3<(status current-filename)
# CHECK: 0 1 2 3
