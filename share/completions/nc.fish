# There a several different implementations of netcat.
# Try to figure out which is the current used one
# and load the right set of completions.

set -l netcat (basename (realpath (which nc)))
switch $netcat
case busybox
    complete -c nc -s l -d "Listen mode, for inbound connects"
    complete -c nc -s p -x -d "Local port"
    complete -c nc -s w -x -d "Connect timeout"
    complete -c nc -s i -x -d "Delay interval for lines sent"
    complete -c nc -s f -r -d "Use file (ala /dev/ttyS0) instead of network"
    complete -c nc -s e -r -d "Run PROG after connect"
case nc.traditional nc.openbsd ncat
    complete -c nc -w $netcat
case '*'
    # fallback to the most restricted one
    complete -c nc -w nc.traditional
end
