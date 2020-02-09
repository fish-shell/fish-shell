if test (uname) = Darwin
    complete -c uname -s a -d 'Behave as though all of the options mnrsv were specified.'
    complete -c uname -s m -d 'print the machine hardware name.'
    complete -c uname -s n -d 'print the nodename'
    complete -c uname -s p -d 'print the machine processor architecture name.'
    complete -c uname -s r -d 'print the operating system release.'
    complete -c uname -s s -d 'print the operating system name.'
    complete -c uname -s v -d 'print the operating system version.'
else
    complete -c uname -s a -l all -d "Print all information"
    complete -c uname -s s -l kernel-name -d "Print kernel name"
    complete -c uname -s n -l nodename -d "Print network node hostname"
    complete -c uname -s r -l kernel-release -d "Print kernel release"
    complete -c uname -s v -l kernel-version -d "Print kernel version"
    complete -c uname -s m -l machine -d "Print machine name"
    complete -c uname -s p -l processor -d "Print processor"
    complete -c uname -s i -l hardware-platform -d "Print hardware platform"
    complete -c uname -s o -l operating-system -d "Print operating system"
    complete -c uname -l help -d "Display help and exit"
    complete -c uname -l version -d "Display version and exit"
end
