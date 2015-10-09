set -q fish_prompt_pwd_abbr; or set -U fish_prompt_pwd_abbr 0

function prompt_pwd --description "Print the current working directory, shortened to fit the prompt"
	set realhome ~
	string replace -r '^'"$realhome"'($|/)' '~$1' $PWD | string replace -ar '([^/.])([^/]{0,'"$fish_prompt_pwd_abbr"'})[^/]*/' '$1$2/'
end
