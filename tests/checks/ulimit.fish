#RUN: %fish %s

ulimit --core-size
#CHECK: {{unlimited|\d+}}
ulimit --core-size 0
ulimit --core-size
#CHECK: 0

ulimit 4352353252352352334
#CHECKERR: ulimit: Invalid limit '4352353252352352334'
#CHECKERR:
#CHECKERR: {{.*}}checks/ulimit.fish (line {{\d+}}):
#CHECKERR: ulimit 4352353252352352334
#CHECKERR: ^
#CHECKERR: (Type 'help ulimit' for related documentation)

# Try to increase a hard limit
# Since limits vary between OSes and distributions, we need to find
# a non-unlimited dynamically
set -l ulimit_opts -c -d -f -n -s -t -u
for ulimit_opt in $ulimit_opts
    set -l max (ulimit -H $ulimit_opt 2>/dev/null)
    if contains -- "$max" "" unlimited
        continue
    end
    set found true
    echo "Using $ulimit_opt to increase hard limit" >&2
    ulimit -H $ulimit_opt (math $max + 1) >/dev/null
    and if __fish_is_cygwin
        echo "ulimit: Permission denied when changing resource of type 'Fake result for  Cygwin'" >&2
    end
    break
end
if not set -q found
    echo "All common hard ulimits are unlimited, cannot increase them" >&2
end
#CHECKERR: Using {{-.}} to increase hard limit
#CHECKERR: ulimit: Permission denied when changing resource of type '{{.+}}'

# Try to lower a hard limit below its soft one
set -e found
for ulimit_opt in $ulimit_opts
    set -l min (ulimit -S $ulimit_opt 2>/dev/null)
    if contains -- "$min" "" 0
        continue
    end
    if test $min = unlimited
        set min 1024
    end
    set found true
    echo "Using $ulimit_opt to lower hard limit" >&2
    ulimit -H $ulimit_opt (math $min - 1) >/dev/null
    and if test (__fish_uname) = FreeBSD
        echo "ulimit: FreeBSD doesn't fail unlike other platforms, so fake it" >&2
    end
    break
end
if not set -q found
    echo "All common soft ulimits are 0, cannot decrease hard ulimits below them" >&2
end
#CHECKERR: Using {{-.}} to lower hard limit
#CHECKERR: ulimit: {{.+}}

if test (__fish_uname) = Linux
    ulimit -K 1024
else
    ulimit -q 1024
end
#CHECKERR: ulimit: Resource limit not available on this operating system
#CHECKERR: {{.*}}checks/ulimit.fish (line {{\d+}}):
#CHECKERR: ulimit -{{.}} 1024
#CHECKERR: ^
#CHECKERR: (Type 'help ulimit' for related documentation)

ulimit --core-size 0 1 2
#CHECKERR: ulimit: too many arguments
#CHECKERR: {{.*}}checks/ulimit.fish (line {{\d+}}):
#CHECKERR: ulimit --core-size 0 1 2
#CHECKERR: ^
#CHECKERR: (Type 'help ulimit' for related documentation)

ulimit --core-size abc
#CHECKERR: ulimit: Invalid limit 'abc'
#CHECKERR: {{.*}}checks/ulimit.fish (line {{\d+}}):
#CHECKERR: ulimit --core-size abc
#CHECKERR: ^
#CHECKERR: (Type 'help ulimit' for related documentation)
