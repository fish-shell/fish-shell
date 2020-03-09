#RUN: %fish -C "set helper %fish_test_helper" %s

# Ensure that if child processes SIGINT, we exit our loops
# Test for #3780

echo About to sigint
#CHECK: About to sigint

while true
    sh -c 'echo Here we go; sleep .25; kill -s INT $$'
end
#CHECK: Here we go

echo I should not be printed because of the SIGINT.
