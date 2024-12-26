# RUN: %fish -C "set helper %fish_test_helper" %s
#REQUIRES: command -v %fish_test_helper

# Check that we don't leave stray FDs.

set -l fds ($helper print_fds)
test "$fds" = "0 1 2"
or begin
    echo This test needs to have all fds other than 0 1 and 2 closed before running >&2
    echo Please close the other fds and try again >&2
    exit 1
end

$helper print_fds
# CHECK: 0 1 2

$helper print_fds 0>&-
# CHECK: 1 2

$helper print_fds 0>&- 2>&-
# CHECK: 1

false | $helper print_fds 0>&-
# CHECK: 0 1 2

$helper print_fds </dev/null
# CHECK: 0 1 2

$helper print_fds </dev/null
# CHECK: 0 1 2

$helper print_fds 3</dev/null
# CHECK: 0 1 2 3

$helper print_fds 5>&2
# CHECK: 0 1 2 5

# This attempts to trip a case where the file opened in fish
# has the same fd as the redirection. In this case, the dup2
# does not clear the CLO_EXEC bit.

$helper print_fds 4</dev/null
# CHECK: 0 1 2 4

$helper print_fds 5</dev/null
# CHECK: 0 1 2 5

$helper print_fds 6</dev/null
# CHECK: 0 1 2 6

$helper print_fds 7</dev/null
# CHECK: 0 1 2 7

$helper print_fds 8</dev/null
# CHECK: 0 1 2 8

$helper print_fds 9</dev/null
# CHECK: 0 1 2 9

$helper print_fds 10</dev/null
# CHECK: 0 1 2 10

$helper print_fds 11</dev/null
# CHECK: 0 1 2 11

$helper print_fds 12</dev/null
# CHECK: 0 1 2 12

$helper print_fds 13</dev/null
# CHECK: 0 1 2 13

$helper print_fds 14</dev/null
# CHECK: 0 1 2 14

$helper print_fds 15</dev/null
# CHECK: 0 1 2 15

$helper print_fds 16</dev/null
# CHECK: 0 1 2 16

$helper print_fds 17</dev/null
# CHECK: 0 1 2 17

$helper print_fds 18</dev/null
# CHECK: 0 1 2 18

$helper print_fds 19</dev/null
# CHECK: 0 1 2 19

$helper print_fds 20</dev/null
# CHECK: 0 1 2 20
