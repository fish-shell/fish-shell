#RUN: %ghoti %s
function commandline
    if test $argv[1] = -ct
        echo --long4\n-4
    else if test $argv[1] = -cpo
        echo cmd\n-z\n-bc\n--long1\narg1\n-d\narg2\n--long2
    end
end

__ghoti_contains_opt -s z
or echo fails to find -z

__ghoti_contains_opt -s c
or echo fails to find -c

__ghoti_contains_opt -s x
and echo should not have found -x

__ghoti_contains_opt -s x -s z
or echo fails to find -z

__ghoti_contains_opt -s x -s c
or echo fails to find -c

__ghoti_contains_opt -s x long1
or echo fails to find --long1

__ghoti_contains_opt long2
or echo fails to find --long2

__ghoti_contains_opt long1 long2
or echo fails to find --long1 or --long2

__ghoti_contains_opt long3
and echo should not have found --long3

__ghoti_contains_opt -s 4 long4
or echo fails to find -4

__ghoti_contains_opt long4
and echo should not have found --long4

__ghoti_contains_opt arg1
and echo should not have found --arg1

__ghoti_contains_opt -s a
and echo should not have found -a

# This should result in message written to stderr and an error status.
__ghoti_contains_opt -x w
and '"__ghoti_contains_opt -x w" should not have succeeded'
#CHECKERR: __ghoti_contains_opt: Unknown option -x

__ghoti_not_contain_opt -s z
and echo should not have found -z

__ghoti_not_contain_opt -s c
and echo should not have found -c

__ghoti_not_contain_opt -s x
or echo unexpectedly found -x

__ghoti_not_contain_opt -s x -s z
and echo should not have found -x/-z

__ghoti_not_contain_opt -s x -s c
and echo should not have found -x/-c

__ghoti_not_contain_opt -s x long1
and echo should not have found --long1

__ghoti_not_contain_opt long2
and echo found --long2

__ghoti_not_contain_opt long1 long2
and echo found --long1 or --long2

__ghoti_not_contain_opt long3
or echo unexpectedly found --long3

__ghoti_not_contain_opt -s 4 long4
and echo unexpectedly found -4

__ghoti_not_contain_opt long4
or echo should not have found --long4

__ghoti_not_contain_opt arg1
or echo should not have found --arg1

__ghoti_not_contain_opt -s a
or echo should not have found -a

# This should result in message written to stderr and an error status.
__ghoti_not_contain_opt -x w
and '"__ghoti_not_contain_opt -x w" should not have succeeded'
#CHECKERR: __ghoti_not_contain_opt: Unknown option -x

true
