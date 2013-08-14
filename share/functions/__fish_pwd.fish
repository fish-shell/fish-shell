if test (uname -o) = Cygwin
	function __fish_pwd --description "Show current path"
		pwd | sed -e 's-^/cygdrive/\(.\)/\?-\u\1:/-'
	end
else
	function __fish_pwd --description "Show current path"
		pwd
	end
end