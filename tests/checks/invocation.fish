#RUN: %fish -C 'set -l fish %fish' %s

$fish -c "echo 1.2.3.4."
# CHECK: 1.2.3.4.

PATH= $fish -c "echo (command a)" 2>/dev/null
echo $status
# CHECK: 255
