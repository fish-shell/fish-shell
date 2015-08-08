set -l args_pre
set -l args_post
switch (uname)
case Darwin
	set args_pre $args_pre -e 's|^/private/|/|'
case 'CYGWIN_*'
	set args_pre $args_pre -e 's|^/cygdrive/\(.\)|\1/:|'
	set args_post $args_post -e 's-^\([^/]\)/:/\?-\u\1:/-'
end

function prompt_pwd -V args_pre -V args_post --description "Print the current working directory, shortened to fit the prompt"
	set -l realhome ~
	echo $PWD | sed -e "s|^$realhome\$|~|" -e "s|^$realhome/|~/|" $args_pre -e 's-\([^/.]\)[^/]*/-\1/-g' $args_post
end
