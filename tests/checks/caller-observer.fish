#RUN: %ghoti %s

# Verify the '--on-job-exit caller' misfeature.
function make_call_observer -a type
    function observer_$type --on-job-exit caller --inherit-variable type
        echo "Observed exit of $type"
    end
end

command echo "echo ran 1" (make_call_observer external)
#CHECK: echo ran 1
#CHECK: Observed exit of external

builtin echo "echo ran 2" (make_call_observer builtin)
#CHECK: echo ran 2
#CHECK: Observed exit of builtin

function func_echo
    builtin echo $argv
end

func_echo "echo ran 3" (make_call_observer function)
#CHECK: echo ran 3
#CHECK: Observed exit of function

# They should not fire again.
# TODO: We don't even clean up the functions!

command echo "echo ran 4"
builtin echo "echo ran 5"
func_echo "echo ran 6"
#CHECK: echo ran 4
#CHECK: echo ran 5
#CHECK: echo ran 6
