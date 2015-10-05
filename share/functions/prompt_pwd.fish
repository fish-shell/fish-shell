function prompt_pwd --description "Print the current working directory, shortened to fit the prompt"
	set realhome ~
	string replace -r '^'"$realhome"'($|/)' '~$1' $PWD | string replace -ar '([^/.])[^/]*/' '$1/'
end
