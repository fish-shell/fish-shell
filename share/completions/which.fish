if which -v > /dev/null ^ /dev/null # GNU
    complete -c which -s a -l all --description "Print all matching executables in PATH, not just the first"
    complete -c which -s i -l read-alias --description "Read aliases from stdin, reporting matching ones on stdout"
    complete -c which -l skip-alias --description "Ignore option '--read-alias'"
    complete -c which -l read-functions --description "Read shell function definitions from stdin, reporting matching ones on stdout"
    complete -c which -l skip-functions --description "Ignore option '--read-functions'"
    complete -c which -l skip-dot --description "Skip directories in PATH that start with a dot"
    complete -c which -l skip-tilde --description "Skip directories in PATH that start with a tilde and executables which reside in the HOME directory"
    complete -c which -l show-dot --description "If a directory in PATH starts with a dot and a matching executable was found for that path, then print './programname'"
    complete -c which -l show-tilde --description "Output a tilde when a directory matches the HOME directory"
    complete -c which -l tty-only --description "Stop processing options on the right if not on tty"
    complete -c which -s v -s V -l version --description "Display version and exit"
    complete -c which -l help --description "Display help and exit"
else # OSX
    complete -c which -s a --description "Print all matching executables in PATH, not just the first"
    complete -c which -s s --description "Print no output, only return 0 if found"
end

complete -c which -a "(complete -C(commandline -ct))" -x
