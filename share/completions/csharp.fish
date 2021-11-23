complete --command csharp --long-option 'attach' --no-files --require-parameter --arguments '(ps -A | awk \'NR > 1 { printf "%s\t%s\n", $1, $4 }\')' --description 'Inject into other processes'
complete --command csharp --short-option 'e' --no-files --require-parameter --description 'Specify expression to execute'
complete --command csharp --short-option 's' --require-parameter --description 'Use file to execute'
