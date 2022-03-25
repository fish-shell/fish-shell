complete -c csharp -l attach -f -r -a '(ps -A | awk \'NR > 1 { printf "%s\t%s\n", $1, $4 }\')' -d 'Inject into other processes'
complete -c csharp -s e -f -r -d 'Specify expression to execute'
complete -c csharp -s s -r -d 'Use file to execute'
