#RUN: fish=%fish %fish %s

# fish_exit fires successfully.
echo 'function do_exit --on-event fish_exit; echo "fish_exiting $fish_pid"; end' > /tmp/test_exit.fish
$fish /tmp/test_exit.fish
# CHECK: fish_exiting {{[0-9]+}}

rm /tmp/test_exit.fish
