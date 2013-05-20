if test (uname) = Darwin
  complete -c uname -s a --description 'Behave as though all of the options mnrsv were specified.'
  complete -c uname -s m --description 'print the machine hardware name.'
  complete -c uname -s n --description 'print the nodename'
  complete -c uname -s p --description 'print the machine processor architecture name.'
  complete -c uname -s r --description 'print the operating system release.'
  complete -c uname -s s --description 'print the operating system name.'
  complete -c uname -s v --description 'print the operating system version.'
else
  complete -c uname -s a -l all --description "Print all information"
  complete -c uname -s s -l kernel-name --description "Print kernel name"
  complete -c uname -s n -l nodename --description "Print network node hostname"
  complete -c uname -s r -l kernel-release --description "Print kernel release"
  complete -c uname -s v -l kernel-version --description "Print kernel version"
  complete -c uname -s m -l machine --description "Print machine name"
  complete -c uname -s p -l processor --description "Print processor"
  complete -c uname -s i -l hardware-platform --description "Print hardware platform"
  complete -c uname -s o -l operating-system --description "Print operating system"
  complete -c uname -l help --description "Display help and exit"
  complete -c uname -l version --description "Display version and exit"
end
