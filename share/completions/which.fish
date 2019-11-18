if which -v >/dev/null 2>/dev/null # GNU
    complete -c which -s a -l all -d "Print all matching executables in PATH, not just the first"
    complete -c which -s i -l read-alias -d "Read aliases from stdin, reporting matching ones on stdout"
    complete -c which -l skip-alias -d "Ignore option '--read-alias'"
    complete -c which -l read-functions -d "Read shell function definitions from stdin, reporting matching ones on stdout"
    complete -c which -l skip-functions -d "Ignore option '--read-functions'"
    complete -c which -l skip-dot -d "Skip directories in PATH that start with a dot"
    complete -c which -l skip-tilde -d "Skip directories in PATH that start with a tilde and executables which reside in the HOME directory"
    complete -c which -l show-dot -d "If a directory in PATH starts with a dot and a matching executable was found for that path, then print './programname'"
    complete -c which -l show-tilde -d "Output a tilde when a directory matches the HOME directory"
    complete -c which -l tty-only -d "Stop processing options on the right if not on tty"
    complete -c which -s v -s V -l version -d "Display version and exit"
    complete -c which -l help -d "Display help and exit"
else # OSX
    complete -c which -s a -d "Print all matching executables in PATH, not just the first"
    complete -c which -s s -d "Print no output, only return 0 if found"
end

complete -c which -a "(__fish_complete_external_command)" -x
