set -l s1
set -l r1
switch (uname)
case Darwin
	set s1 '^/private/'
	set r1 /
case 'CYGWIN_*'
	set s1 '^/cygdrive/(.)'
	set r1 '$1:'
end

function prompt_pwd -V s1 -V r1 --description "Print the current working directory, shortened to fit the prompt"
	set realhome ~
	string replace -r '^'"$realhome"'($|/)' '~$1' $PWD | string replace -r "$s1" "$r1" | string replace -ar '([^/.])[^/]*/' '$1/'
end
