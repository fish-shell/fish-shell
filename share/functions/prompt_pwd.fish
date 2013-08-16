switch (uname)
case Darwin
	function prompt_pwd --description "Print the current working directory, shortend to fit the prompt"
		echo $PWD | sed -e "s|^$HOME|~|" -e 's|^/private||' -e 's-\([^/.]\)[^/]*/-\1/-g'
	end
case 'CYGWIN_*'
	function prompt_pwd --description "Print the current working directory, shortend to fit the prompt"
		echo $PWD | sed -e "s|^$HOME|~|" -e 's|^/cygdrive/\(.\)|\1/:|' -e 's-\([^/.]\)[^/]*/-\1/-g' -e 's-^\([^/]\)/:/\?-\u\1:/-'
	end
case '*'
	function prompt_pwd --description "Print the current working directory, shortend to fit the prompt"
		echo $PWD | sed -e "s|^$HOME|~|" -e 's-\([^/.]\)[^/]*/-\1/-g'
	end
end
