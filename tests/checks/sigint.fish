#RUN: %fish -C "set helper %fish_test_helper" %s

# Ensure we can break from a while loop.

echo About to sigint
$helper sigint_parent &
while true
end
echo I should not be printed because I got sigint

#CHECK: About to sigint
#CHECKERR: Sent SIGINT to {{\d*}}
