
function __fish_print_function_prototypes -d "Prints the names of all function prototypes found in the headers in the current directory"
    set -l headers *.h *.hh *.hpp *.hxx
    if set -q headers[1]
        sed -n "s/^\(.*[^[a-zA-Z_0-9]\|\)\([a-zA-Z_][a-zA-Z_0-9]*\) *(.*);.*\$/\2/p" $headers
    end
end

