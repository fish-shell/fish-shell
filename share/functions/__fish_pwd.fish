switch (uname)
case 'CYGWIN_*'
	function __fish_pwd --description "Show current path"
		pwd | sed -e 's-^/cygdrive/\(.\)/\?-\u\1:/-'
	end
case '*'
	function __fish_pwd --description "Show current path"
		pwd
	end
end