if seq --version 2>/dev/null >/dev/null #GNU
    complete -c seq -s f -l format -d 'Use printf style floating-point FORMAT'
    complete -c seq -s s -l separator -d 'Use STRING to separate numbers'
    complete -c seq -s w -l equal-width -d 'Equalize width with leading zeroes'
    complete -c seq -l help -d 'Display this help'
    complete -c seq -l version -d 'Output version information'
else #OS X
    complete -c seq -s f -d 'Use printf style floating-point FORMAT'
    complete -c seq -s s -d 'Use STRING to separate numbers'
    complete -c seq -s w -d 'Equalize width with leading zeroes'
    complete -c seq -s t -d 'Use STRING to terminate sequence of numbers'
end
