# RUN: env ghoti_test_helper=%ghoti_test_helper %ghoti %s

# Ensure that a job which attempts to disown itself does not explode.
# Here ghoti_test_helper is the process group leader; we attempt to disown
# its pid within a pipeline containing it.

function disowner
    read -l pid
    echo Disown $pid
    disown $pid
end
$ghoti_test_helper print_pid_then_sleep | disowner

# CHECK: Disown {{\d+}}
