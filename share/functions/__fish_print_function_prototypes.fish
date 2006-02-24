
function __fish_print_function_prototypes -d "Prints the names of all function prototypes found in the headers in the current directory"
	cat *.h*|sed -n "s/^\(.*[^[a-zA-Z_0-9]\|\)\([a-zA-Z_][a-zA-Z_0-9]*\) *(.*);.*\$/\2/p"
end

